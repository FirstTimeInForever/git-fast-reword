#ifndef WRAPPERS_HPP
#define WRAPPERS_HPP

#pragma once

#include <git2.h>
#include <cassert>
#include <string>
#include <stdexcept>
#include <exception>
#include <algorithm>


struct git_exception: public std::runtime_error {
    int error_code = GIT_OK;

    using std::runtime_error::runtime_error;

    git_exception(const std::string& message, int error_code):
        std::runtime_error(message), error_code(error_code) {}
};

inline void check_error(int error) {
    if (error != GIT_OK) {
        throw git_exception("libgit call returned error", error);
    }
}

namespace wrappers {
    template<class resource_type>
    class generic_wrapper {
    public:

        using deleter_type = void(resource_type*);

        explicit generic_wrapper(deleter_type deleter) noexcept: deleter(deleter) {}

        generic_wrapper(resource_type* resource, deleter_type deleter) noexcept:
            resource(resource), deleter(deleter) {}

        generic_wrapper(const generic_wrapper&) = delete;
        generic_wrapper& operator=(const generic_wrapper&) = delete;

        generic_wrapper(generic_wrapper&& other)  noexcept {
            swap(other);
        }

        generic_wrapper& operator=(generic_wrapper&& other) noexcept {
            this->~generic_wrapper();
            swap(other);
            return *this;
        }

        void swap(generic_wrapper& other) noexcept {
            std::swap(resource, other.resource);
            std::swap(deleter, other.deleter);
        }

        resource_type*& get() noexcept {
            return resource;
        }

        [[nodiscard]]
        resource_type* const& get() const noexcept {
            return resource;
        }

        virtual ~generic_wrapper() noexcept {
            if (resource) {
                std::invoke(deleter, resource);
                resource = nullptr;
            }
        }

    protected:

        resource_type* resource{};
        deleter_type* deleter;
    };

    class repository: public generic_wrapper<git_repository> {
    public:

        repository() noexcept: generic_wrapper<git_repository>(git_repository_free) {}

        explicit repository(const std::string& path): generic_wrapper<git_repository>(git_repository_free) {
            auto error = git_repository_open(&resource, path.c_str());
            if (error != GIT_OK) {
                throw git_exception("Could not open repository!", error);
            }
        }

        repository(const repository&) = delete;
        repository& operator=(const repository&) = delete;

        repository(repository&& other) noexcept: generic_wrapper<git_repository>(std::move(other)) {}

        repository& operator=(repository&& other) noexcept {
            generic_wrapper<git_repository>::operator=(std::move(other));
            return *this;
        }
    };

    class commit: public generic_wrapper<git_commit> {
    public:

        commit() noexcept: generic_wrapper<git_commit>(git_commit_free) {}

        commit(const git_oid& oid, const repository& repository): generic_wrapper<git_commit>(git_commit_free) {
            auto error = git_commit_lookup(&resource, repository.get(), &oid);
            if (error != GIT_OK){
                throw git_exception("Failed to lookup commit object!", error);
            }
        }

        explicit commit(git_commit* pointer) noexcept: generic_wrapper<git_commit>(pointer, git_commit_free) {}

        commit(const commit&) = delete;
        commit& operator=(const commit&) = delete;

        commit(commit&& other) noexcept: generic_wrapper<git_commit>(std::move(other)) {}

        commit& operator=(commit&& other) noexcept {
            generic_wrapper<git_commit>::operator=(std::move(other));
            return *this;
        }
    };

    inline auto make_reference() noexcept {
        return generic_wrapper<git_reference>(git_reference_free);
    }

    inline auto make_revwalker() noexcept {
        return generic_wrapper<git_revwalk>(git_revwalk_free);
    }

    inline auto make_object() noexcept {
        return generic_wrapper<git_object>(git_object_free);
    }

    inline auto make_tree() noexcept {
        return generic_wrapper<git_tree>(git_tree_free);
    }
}

#endif
