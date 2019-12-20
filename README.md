# Branch search

A simple terminal branch search for git

![example](img.gif)

## Usage

`bs` accepts the following command line arguments:

- `-p`: checkout to the selected branch and `git pull` afterwards.

- `-u`: `git fetch` before showing the branches.

- `-l`: list local branches only.

- `[pattern]`: if provided, a `git checkout` is immediately performed to a branch containing the pattern. For instance, `bs maste` will most likely switch to `master`.

## Installation

The only needed dependency is `ncurses`. You can read
how to install it in most distros in [this tutorial](https://www.osetc.com/en/how-to-install-ncurse-library-in-ubuntu-debian-centos-fedora-linux.html). Once installed just run:


```
sudo make install
```

That will compile the thing and copy the binary to `/bin/`. If you prefer, you may just run `make` and copy the generated exectable (`bs`) to your preferred location.

## FAQ

- This could have been a simple python script

First of all, how dare you. Secondly, that's not even a question. A python script wouldn't be as fun.
