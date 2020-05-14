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
    class repository {
    public:

        repository() noexcept = default;

        explicit repository(const std::string& path) {
            auto error = git_repository_open(&pointer, path.c_str());
            if (error != GIT_OK) {
                throw git_exception("Could not open repository!", error);
            }
        }

        repository(const repository&) = delete;
        repository& operator=(const repository&) = delete;

        repository(repository&& other) noexcept {
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

        [[nodiscard]]
        git_repository* const& get() const noexcept {
            return pointer;
        }

        void swap(repository& other) noexcept {
            std::swap(pointer, other.pointer);
        }

        ~repository() noexcept {
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
            auto error = git_commit_lookup(&pointer, repository.get(), &oid);
            if (error != GIT_OK){
                throw git_exception("Failed to lookup commit object!", error);
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

        [[nodiscard]]
        git_commit* const& get() const noexcept {
            return pointer;
        }

        void swap(commit& other) noexcept {
            std::swap(pointer, other.pointer);
        }

        ~commit() noexcept {
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

        explicit generic_wrapper(deleter_type deleter) noexcept: deleter(deleter) {}

        generic_wrapper(resource_type* resource, deleter_type deleter) noexcept:
            resource(resource), deleter(deleter) {}

        resource_type*& get() noexcept {
            return resource;
        }

        [[nodiscard]]
        resource_type* const& get() const noexcept {
            return resource;
        }

        ~generic_wrapper() noexcept {
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

    inline auto make_object() noexcept {
        return generic_wrapper<git_object>(git_object_free);
    }
}

#endif
