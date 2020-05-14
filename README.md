# git-fast-reword

Fast alternative for changing commit messages.

## Motivation

Given a big git repository and some commit `C`, let's say something like `HEAD~1000`.
You want to change commit message of `C`. So, what you can do is:

```
git rebase -i HEAD~1000
```

The problem is, that `git rebase` is extremely slow if you want apply it to 1000 commits.
So, instead you can use 
```
git-fast-reword HEAD~1000 "You new message"
```
to solve the problem.

## Build

You will need `C` and `C++` compilers able to compile `C99` and `C++17` code.
You will also need `cmake` and `libssl` (on ubuntu: `apt install cmake libssl-dev`).

```
git clone https://github.com/FirstTimeInForever/git-fast-reword.git --recursive
cd git-fast-reword
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ../ && make all -j8
```

## How it works

This tool will find target commit specified by `<revision>`, find all commits in range `[<revision>; HEAD]`, reword the target commit and recreate the chain of child commits from the range.
After that it sets current `HEAD` to recreated commit.

## Usage

```
git-rebase-reword <revision> <message> [--verbose]
```
