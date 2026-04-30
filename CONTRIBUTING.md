<b>Development Process</b>

Contributors should work from their own fork of the repository (not push branches directly to the upstream repo unless they are maintainers with that workflow). When you start a change, create a branch named `<id-3letter>/<topic>`: use a three-letter identifier derived from your name or handle, then a short topic (often kebab-case words). Examples for someone named John Doe: `jdo/fix`, `doe/dependencies`.

Open pull requests against the upstream **`development`** branch when you consider your feature or bug fix ready.

The patch will be accepted if there is broad consensus that it is a good thing. Developers should expect to rework and resubmit patches if they don't match the project's coding conventions or are controversial.

The `development` branch is regularly built and tested, but is not guaranteed to be completely stable. Tags are regularly created to indicate new official, stable release versions of Conceal.

Feature branches on upstream may be created when there are major new features being worked on by several people.

From time to time a pull request will become outdated. If this occurs, and the pull is no longer automatically mergeable; a comment on the pull will be used to issue a warning of closure. The pull will be closed 15 days after the warning if action is not taken by the author. Pull requests closed in this manner will have their corresponding issue labeled 'stagnant'.

Issues with no commits will be given a similar warning, and closed after 15 days from their last activity. Issues closed in this manner will be labelled 'stale'.
