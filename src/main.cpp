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


inline void check_error(int error) {
    if (error != GIT_OK) {
        throw std::runtime_error("libgit returned error code: " + std::to_string(error));
    }
}

namespace wrappers {
    class repository {
    public:

        repository() noexcept = default;

        explicit repository(const std::string& path) {
            check_error(git_repository_open(&pointer, path.c_str()));
        }

        repository(const repository&) = delete;
        repository& operator=(const repository&) = delete;

        repository(repository&& other)  noexcept {
            swap(other);
        }

        repository& operator=(repository&& other) noexcept {
            this->~repository();
            swap(other);
            return *this;
        }

        git_repository*& get() noexcept {
            return pointer;
        }

        git_repository* const& get() const noexcept {
            return pointer;
        }

        void swap(repository& other) noexcept {
            std::swap(pointer, other.pointer);
        }

        ~repository() {
            if (pointer) {
                git_repository_free(pointer);
                pointer = nullptr;
            }
        }

    private:

        git_repository* pointer{};
    };

    class commit {
    public:

        commit() noexcept = default;

        commit(const git_oid& oid, const repository& repository) {
            if (git_commit_lookup(&pointer, repository.get(), &oid)){
                throw std::runtime_error("Failed to lookup the next object!");
            }
        }

        explicit commit(git_commit* pointer) noexcept: pointer(pointer) {}

        commit(const commit&) = delete;
        commit& operator=(const commit&) = delete;

        commit(commit&& other)  noexcept {
            swap(other);
        }

        commit& operator=(commit&& other) noexcept {
            this->~commit();
            swap(other);
            return *this;
        }

        git_commit*& get() noexcept {
            return pointer;
        }

        git_commit* const & get() const noexcept {
            return pointer;
        }

        void swap(commit& other) noexcept {
            std::swap(pointer, other.pointer);
        }

        ~commit() {
            if (pointer) {
                git_commit_free(pointer);
                pointer = nullptr;
            }
        }

    private:

        git_commit* pointer{};
    };

    template<class resource_type>
    struct generic_wrapper {
        resource_type* resource{};
        using deleter_type = void(resource_type*);
        deleter_type* deleter;

        explicit generic_wrapper(deleter_type deleter): deleter(deleter) {}

        generic_wrapper(resource_type* resource, deleter_type deleter):
            resource(resource), deleter(deleter) {}

        resource_type*& get() noexcept {
            return resource;
        }

        resource_type* const& get() const noexcept {
            return resource;
        }

        ~generic_wrapper() {
            if (resource) {
                std::invoke(deleter, resource);
            }
        }
    };

    inline auto make_reference() noexcept {
        return generic_wrapper<git_reference>(git_reference_free);
    }

    inline auto make_revwalker() noexcept {
        return generic_wrapper<git_revwalk>(git_revwalk_free);
    }
}

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
    if (git_reference_name_to_id(&target, repository.get(), revision_id.c_str()) != GIT_OK) {
        throw std::runtime_error("Could not fetch target revision");
    }
    return target;
}

void stuff(const std::string& revision_id, const std::string& message) {
    wrappers::repository repository(".");
    auto target_oid = get_target_commit(revision_id, repository);
    auto commits = collect_oids(target_oid, repository);
    print_commits_to_recreate(commits, repository);
    if (git_oid_equal(&commits.back(), &target_oid)) {
        commits.pop_back();
    }
    std::reverse(commits.begin(), commits.end());
    std::cout << std::endl;
    auto updated_target = reword_commit(target_oid, message, repository);
    auto parent = recreate_commits(updated_target, commits, repository);
    auto head = wrappers::make_reference();
    git_repository_head(&head.get(), repository.get());
    if (!git_reference_is_branch(head.get())) {
        throw std::runtime_error("HEAD must point to branch");
    }
    auto updated_head = wrappers::make_reference();
    check_error(git_reference_set_target(&updated_head.get(), head.get(), &parent, "reword HEAD update"));
    std::cout << "HEAD -> " << make_oid_str(parent) << std::endl;
}

void show_usage() {
    std::cout << "Usage: " << std::endl;
    std::cout << "git-rebase-reword <revision> <message>" << std::endl;
    std::cout << "\t<revision> - A commit hash or HEAD~N to change commit message" << std::endl;
    std::cout << "\t<message> - New commit message" << std::endl;
    std::cout << std::endl;
    std::cout << "Example: " << std::endl;
    std::cout << "git-rebase-reword 6a8e6e325d6383d64469f377250042b881e05b4c \"Some new message\"" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        show_usage();
        return 1;
    }
    auto start_time = std::chrono::high_resolution_clock::now();
    git_libgit2_init();
    bool got_errors = false;
    try {
        stuff(argv[1], argv[2]);
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
    std::cout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end_time).count() << "ms"<< std::endl;
    return 0;
}