#include <git2.h>
#include <cstdint>
#include <iostream>
#include <cassert>
#include <vector>
#include <array>
#include <string>
#include <stdexcept>
#include <functional>
#include <chrono>

#include "wrappers.hpp"


inline std::string make_oid_str(const git_oid& oid) {
    std::string target;
    target.resize(41);
    git_oid_tostr(target.data(), target.size(), &oid);
    return target;
}

inline std::string inspect_commit_message(const git_oid& oid, const wrappers::repository& repository) {
    wrappers::commit commit(oid, repository);
    std::string result(git_commit_message(commit.get()));
    return result;
}

std::vector<git_oid> collect_oids(const git_oid& target_oid, const wrappers::repository& repository) {
    auto walker = wrappers::make_revwalker();
    check_error(git_revwalk_new(&walker.get(), repository.get()));
    git_revwalk_sorting(walker.get(), GIT_SORT_NONE);
    git_revwalk_push_head(walker.get());
    git_oid current_oid{};
    std::vector<git_oid> result;
    while (git_revwalk_next(&current_oid, walker.get()) != GIT_ITEROVER) {
        result.push_back(current_oid);
        if (git_oid_equal(&current_oid, &target_oid)) {
            break;
        }
    }
    return result;
}

std::vector<git_commit*> get_commit_parents(const git_commit* commit) {
    std::vector<git_commit*> parents;
    auto parents_count = git_commit_parentcount(commit);
    for (size_t it = 0; it < parents_count; ++it) {
        git_commit* parent{};
        if (git_commit_parent(&parent, commit, it) != GIT_OK) {
            throw std::runtime_error("Could not parent of " + make_oid_str(*git_commit_id(commit)));
        }
        parents.push_back(parent);
    }
    return parents;
}

git_oid reword_commit(const git_oid& oid, const std::string& message, const wrappers::repository& repository) {
    wrappers::commit target_commit(oid, repository);
    git_tree* tree{};
    git_commit_tree(&tree, target_commit.get());
    git_oid noid{};
    auto parents = get_commit_parents(target_commit.get());
    auto error = git_commit_create(&noid, repository.get(), nullptr, git_commit_author(target_commit.get()),
        git_commit_committer(target_commit.get()), nullptr, (message + "\n").c_str(), tree,
        git_commit_parentcount(target_commit.get()), (const git_commit**)(parents.data()));
    check_error(error);
    std::cout << make_oid_str(oid) << " -> " << make_oid_str(noid) << std::endl;
    return noid;
}

git_oid recreate_commit_with_parent(const git_oid& oid, const git_oid& parent, const wrappers::repository& repository) {
    wrappers::commit original_commit(oid, repository);
    git_tree* tree{};
    git_commit_tree(&tree, original_commit.get());
    git_oid noid{};
    wrappers::commit parent_commit(parent, repository);
    std::array<const git_commit*, 1> prr{parent_commit.get()};
    auto error = git_commit_create(&noid, repository.get(), nullptr, git_commit_author(original_commit.get()),
        git_commit_committer(original_commit.get()), git_commit_message_encoding(original_commit.get()),
        git_commit_message(original_commit.get()), tree, 1, prr.data());
    check_error(error);
    return noid;
}

git_oid recreate_commits(git_oid parent, std::vector<git_oid>& oids, const wrappers::repository& repository) {
    for (const auto& oid: oids) {
        std::cout << make_oid_str(oid) << " -> ";
        parent = recreate_commit_with_parent(oid, parent, repository);
        std::cout << make_oid_str(parent) << std::endl;
    }
    return parent;
}

void print_commits_to_recreate(const std::vector<git_oid>& commits, const wrappers::repository& repository) {
    std::cout << "Commits to be recreated: " << std::endl;
    for (const auto& commit: commits) {
        auto cm = inspect_commit_message(commit, repository);
        std::cout << make_oid_str(commit) << " " << cm;
        if (cm.back() != '\n') {
            std::cout << std::endl;
        }
    }
}

git_oid get_target_commit(const std::string& revision_id, const wrappers::repository& repository) {
    git_oid target{};
    if (git_oid_fromstr(&target, revision_id.c_str()) == GIT_OK) {
        return target;
    }
    auto ref = wrappers::make_reference();
    auto error = git_reference_name_to_id(&target, repository.get(), revision_id.c_str());
    if (error != GIT_OK) {
        throw std::runtime_error("Could not fetch target revision. Error code: " + std::to_string(error));
    }
    return target;
}

void rebase_reword(const std::string& revision_id, const std::string& message, bool verbose) {
    wrappers::repository repository(".");
    auto head = wrappers::make_reference();
    auto error = git_repository_head(&head.get(), repository.get());
    if (error != GIT_OK) {
        throw std::runtime_error("Could not get repository head!. Error code: " + std::to_string(error));
    }
    if (!git_reference_is_branch(head.get())) {
        throw std::runtime_error("HEAD must point to branch");
    }
    auto target_oid = get_target_commit(revision_id, repository);
    auto commits = collect_oids(target_oid, repository);
    if (verbose) {
        print_commits_to_recreate(commits, repository);
        std::cout << std::endl;
    }
    if (git_oid_equal(&commits.back(), &target_oid)) {
        commits.pop_back();
    }
    std::reverse(commits.begin(), commits.end());
    auto updated_target = reword_commit(target_oid, message, repository);
    auto parent = recreate_commits(updated_target, commits, repository);
    auto updated_head = wrappers::make_reference();
    error = git_reference_set_target(&updated_head.get(), head.get(), &parent, "reword HEAD update");
    if (error != GIT_OK) {
        throw std::runtime_error("Could not set HEAD to " + make_oid_str(parent) + ". Error code: " + std::to_string(error));
    }
    std::cout << "HEAD is now pointing to: " << make_oid_str(parent) << std::endl;
}

void show_usage() {
    std::cout << "Usage: " << std::endl;
    std::cout << "git-rebase-reword <revision> <message> [--verbose]" << std::endl;
    std::cout << "\t<revision> - A commit hash or HEAD~N to change commit message" << std::endl;
    std::cout << "\t<message> - New commit message" << std::endl;
    std::cout << std::endl;
    std::cout << "Example: " << std::endl;
    std::cout << "git-rebase-reword 6a8e6e325d6383d64469f377250042b881e05b4c \"Some new message\"" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 3 || argc > 4) {
        show_usage();
        return 1;
    }
    bool verbose = false;
    if (argc == 4) {
        std::string arg(argv[3]);
        if (arg != "--verbose") {
            show_usage();
            return 1;
        }
        verbose = true;
    }
    auto start_time = std::chrono::high_resolution_clock::now();
    git_libgit2_init();
    bool got_errors = false;
    try {
        rebase_reword(argv[1], argv[2], verbose);
    }
    catch (const std::exception& exception) {
        std::cout << exception.what() << std::endl;
        got_errors = true;
    }
    git_libgit2_shutdown();
    if (got_errors) {
        return 1;
    }
    auto end_time = std::chrono::high_resolution_clock::now() - start_time;
    if (verbose) {
        std::cout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_time).count() << "ms" << std::endl;
    }
    return 0;
}
