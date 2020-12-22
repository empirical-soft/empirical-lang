# Contributor's Notes

## Issues

File an [issue](https://github.com/empirical-soft/empirical-lang/issues) for non-trivial changes. This will be assigned a number, like 123.

## Forking and Cloning

You will want to **fork** your own copy of Empirical. Then you can **clone** your fork locally. See [this tutorial](https://help.github.com/en/github/getting-started-with-github/fork-a-repo).

It will help to add a remote repo back to the official Empirical:

```
$ git remote add upstream git@github.com:empirical-soft/empirical-lang.git
```

You can confirm that the remote repo is available with:

```
$ git remote -v
```

## Branches

Branches make things easy. As a rule of thumb, create a new branch for each issue with the issue number:

```
$ git checkout -b gh123
```

You can confirm that you are on the new branch with:

```
$ git branch
```

## Building and Testing

To build and test Empirical on POSIX, simply run:

```
$ make
$ make test
```

On Windows, run:

```
$ .\make.bat
$ .\make.bat test
```

## Making the Change

Change the code however you want, but do try to adhere to the existing style of the code base. Empirical broadly follows the [Google Style Guide](https://google.github.io/styleguide/cppguide.html) for C++ and [PEP 8](https://www.python.org/dev/peps/pep-0008/) for Python.

## Committing and Pull Requests

You can send your finished changes for consideration. First, commit your work locally:

```
$ git commit -am "Useful statement here explaining the change"
```

Then push the change to your own fork:

```
$ git push origin gh123
```

You should get a message from GitHub alerting you to where you can "create a pull request". Go to the listed URL to issue a **pull request** (PR).

It will help to list which issue this change addresses. For example:

```
fixes #123
```

Making the pull request will trigger testing on all target platforms. Fix any errors that you see and re-push your new changes; they will automatically be reflected in the existing PR.

A maintainer must approve the final changes to be incorporated.

## Final Cleanup

Once your changes have been accepted, you will want to sync all of your repos and remove the branch. This is possible with:

```
$ ./scripts/gh_update.sh gh123
```
