Contributing
============

Contributions are encouraged.   Every little bit helps, and credit will always be given.  This code base is ZOS-specific C code and some knowledge of ZOS probably required.   

Ways to Contribute
----------------------

* Report Issues: Create issue tickets at [zss](https://github.com/zowe/zss/issues)
    * Note: Issues you see may have already been resolved, so please review closed tickets before creation.
* Suggest new features: Create feature request tickets at [zss](https://github.com/zowe/zss/issues)
* Work on existing tickets: Get assigned to and/or submit pull requests for one of the tickets above.
* Community outreach: Answer questions seen in [Zowe Communication Channels](https://github.com/zowe/community/blob/master/README.md#communication-channels), create blogs or videos about our software, and more. This document does not cover media contribution, but it is greatly appreciated.

Get Started Contributing
------------------------

* Set up a development environment by following this [README.md](https://github.com/zowe/zlux-app-server/blob/staging/README.md).
* Read [the wiki](https://github.com/zowe/zlux/wiki) to learn the code
* Get assigned a github issue either by request or self-assignment.
* Complete development
* Create PR against staging branch (Ensure you include update to CHANGELOG.md if required)
    * Use a draft pull request for work in progress that you want to build on the CICD pipeline.
    * Ensure PR checklist completed (Checklist appears when creating PR)
* Inform or assign the relevant [committers] group for review, or specific people you know will be doing review
* Await review
* Update PR based upon review, until approved

## Development Guidelines
-------------------------

### What belongs in this repository?

The zss repo contains the code that is specific to the ZSS (unprivileged) and ZIS (privileged) servers that anchor Zowe's base on ZOS.   They both provide server frameworks with a plugin architecture and the code that defines these belongs in this repo.  Also, services that are considered to be "core" to any deployment of ZSS and ZIS are included here.   For example, a logon service, or a file-serving service.  

The zss repo depends fundamentally on the repo zowe-common-c (https://github.com/zowe-common-c).  Zowe-common-c is intended to be the "big tent" of progamming libraries to make programming in C on ZOS possible beyond what IBM provides "out of the box", which is C99 plus mostly a POSIX runtime and little OS-awareness.   See its documentation for full details, but the general division between these repos is mechanism vs policy.   The lower-level repo, zowe-common-c, defines mechanism for many things such as recovery, logging, io, etc, but does not stipulate how these things should be used to build any specific top-level program.  However the zss repo does.   It defines a very specific set of extensible services that the Zowe APIML and App Framework (aka ZLUX) require.   It has very specific rules about authentication and authorization, configuration, packaging and deployment.  

### What goes in a plugin?

Most of the bulk of the this repo is plugin code for core plugins.   

#### ZSS

ZSS Plugins are REST services. The code that goes in these plugins goes in the code that processes URLs and encodes/decodes JSON.  Also code that is about the zss server itself, goes in these plugins. e.g. the serverStatusService.c.  An example of a typical split is in the zosDiscovery service.   This service is just a wrapper for zowe-common-c/c/discovery.c.  The lower level code may have much use outside of the ZSS as it is today.  And it might empower or enlighten programmers who are looking to program in C on ZOS without requiring the whole Zowe framework.   We want Zowe to be useful to many stakeholders in many ways, like most other good open source citizens.

### ZIS

ZIS Plugins are PC calls.   PC calls are the principal modern means of communication between two ZOS programs in differnt address spaces, or between an address space and the OS itself.   PC Calls are specially built for ensuring clear boundaries of privilege escalation managed by the ESM (i.e. SAF) implementation.  Each defined service of a ZIS Plugin can use the general FACILITY class profile or define a specific profile for a single service.   This allows Zowe security to be managed on as fine-graind a basis as the administrators of a ZOS intance require.   The code that picks apart the PC's function request and parameter block go in th ZIS repo.   Usually the fundamental service is provided in zowe-common-c.   An example could be shown for managing shared memory.  

### 64-bit tranistion

It may be surprising to newcomers to ZOS that a lot of code is still using 31-bit addressing.  Due to historical accident more than intention, ZSS 1.0 went out as a 31-bit program.  Initially ZSS 2.0 supports both, but will not deliver new features necessarily in both modes.  For example, shared memory does not work in 64 bit mode.  If there is a plugin for ZSS in circulation that does not support 64-bit compilation, the ZSS team will help the transition if help is needed.  Please log an issue to get our attention. 

## Communication Guidelines
------------------------

Many guidelines and principles for specific tasks are below, but communication early on will prevent complications later.

* Communicate frequently (before pull request) with cross-team member representatives (in informal & small meetings) for new design features.
* Before implementing new functionality, reach out to squad members and discuss architecture and design.
* Check to see if what you are working on already has an issue ticket that is assigned to someone else. If so, talk to them to learn if you can cooperate or own the task. If there is no issue ticket, create one and inform the Web UI squad to request assignment to it.

## Branch Guidelines
------------------------

We use a fork-and-pull model for pull requests, in which your pull request should be done on a fork of a Zowe repository, and a pull request made from a branch on that fork to a branch on our code.

Zowe has a few important branches,
* master: The branch for released code. Never commit or make a pull request here, as this is the stable branch.
* rc: The branch for validating code for release during a code freeze period. Only make a pull request here if you are fixing something after code freeze.
* staging: The primary branch for development. Unless otherwise noted, always make pull requests to here.

For organization, you may want to use these branch prefixes on your fork:
* test/: For draft PRs that you aren't sure about or don't want to commit.
* feature/: For improvements to a repository
* bugfix/: For fixes to a repository
Other branch names are possible, such as if you are doing a group project and want to have a meaningful name for the task.

## PR Guidelines
------------------------
* Do not implement more than one feature or fix in the same PR. Problems in one feature can hold back merging of other good features.
   * But don't spread an implementation out over too many PRs either, find the right balance.
   * Do keep the number of commits small (possibly by doing git squash) to make the PR easier to review
* Avoid making a PR that is based upon another PR in the same repo, as if the first is rejected it will cause the second to need changes.
   * Sometimes it is good to make a PR to merge your work into an open PR, essentially combining them.
* Each repo has a PR template, check that your PR conforms to the tasks in the template to ensure easy PR approval


## Changelog Update Guidelines
------------------------

Add an entry to changelog.md for any PR that introduces a feature, enhancement, or fix that affects end users. Changes to certain files, such as the Jenkinsfile, do not require a changelog update. The changelogs are compiled into Zowe Docs [Release Notes](https://docs.zowe.org/stable/getting-started/summaryofchanges.html) periodically.

**Each changelog entry must:** Describe the change and how it impacts end users. If it doesn't impact the users, it doesn't need to be in the changelog.

## Code Guidelines

### Use of editors
------------------------

A good editor is one that enables you to code fast, locate code fast, and understand a codebase better. Master the editor of your choice to develop efficiently.
Never use editor options that reformat the contents of entire files automatically. This leads to hard to read pull requests, and unnecessarily altered history.


### Whitespacing, syntax, and linting
------------------------

* Do not add or delete white spaces in code unless you are the owner of the code. It alters the history of ownership of lines in git, making it time consuming to diagnose bugs.
* Tabs should never be in code, except in the case above.
* Use 2 space indentation for new code, but respect pre-existing indentation rules of a repository or file. If a file uses 4 spaces, new code in that file should as well to be consistent.
* Indentation rules apply to generated code too, unless there are software constraints or the purpose is minification. Otherwise, don't assume that a generated file won't eventually be read by a person.
* Linting is good but do not add linters or change linter rules without agreement by the repository committers, as it effects all preexisting code and how the committers contribute.
* Braces should be used for conditionals for readability.
Example,

Right:

```
if (condition) {
  consequent;
} else {
  alternate;
}
```

Wrong:

```
if (condition) consequent;
if (condition)
  consequent;
if (condition)
  consequent;
else alternate;
```

### Commenting guidelines
------------------------

* Comment areas of code that are not intuitive, are complex, or where there is uncertainty and unknowns.
    * Excessive comments can be as bad as no comments, so utilize well-named variables and functions to avoid the need for extra comments.
* Write for your future self and your future collaborators. Would you remember how something worked or why it was written a certain way 3 years from when it was written?
* Do not commit commented out code, unless it is expected to be used very soon, or is an example.
* TODO and other categorical comments are fine, and help others to enhance the code later

### Naming standards
------------------------

* Classes are always nouns
* Interfaces are nouns or adjectives
* Methods and Functions are always verbs or verb clauses
* No abbreviations. It is always better to spell it out, only widely accepted abbreviations like URL are allowed
* Use camelCase variables and methods
* Use PascalCase for classes and functions
* Use ALL_CAPS for constants (except local consts in JS, use camelCase)
* IDs should start with org.zowe or org-zowe as appropriate to the language, and for the frameworks use org.zowe.zlux and org.zowe.zss

### File Naming Guidelines
------------------------

Capitalizing each word without spaces (PascalCase) and lower case with dashes (dash-case) are typically used within our project and recommended for new repositories.
However, it's most important that you follow the naming convention of whatever is already in the codebase you are working with.

### Versioning Guidelines
------------------------

* Anywhere a version is shown, it must follow the major.minor.patch rules of semantic versioning (semver.org)
* Long-term viability of a protocol depends upon having version numbers included in messages so that two modules can adjust behavior depending on versions encountered.
    * Examples: telnet, HTTPS, SSH. They all have initial "handshakes" where the participants find a compatible set of parameters to use
* Versions (and lengths) should be present within data structures
* API versioning: When a new argument for a function is needed, do not add it to the existing function unless absolutely necessary as it will break callers. Instead, make a new function with a similar name, and relate them.
   * If myFunction(arg1, arg2) needs an arg3, make myFunction2(arg1, arg2, arg3) and if possible, edit myFunction to have it call myFunction2 with a default value for arg3.


### Use of 3rd party libraries
------------------------

Because ZSS is a c language library that is very specific to Zowe's back-end services running on ZOS it is not highly likely that 3rd party libraries will play a large role in its development.  However, there may be exceptions.

Squad committers look for the following traits in a good library:

* Provenance: Who made this library? Can we expect this library to exist in the future, or could it be taken down or abandoned?
* Maintenance record: Is this library active, and are issues addressed in a timely manner?
* Performance: Does this library perform well relative to alternatives?
* Popularity: Is this library used by other well-known programs? This is not as important as provenance and maintenance record, but can be related.
* Complexity: Is this something you would not be able to write, or would not write as well yourself?
* Licensing: Are we allowed to use this library? Is it compatible with our license? (EPL-2.0)
* Documentation & comprehension: Is this library well-documented, easy to understand or is the code well-organized? In the case a library does become abandoned or bug patches need to be submitted, would it be easy to do?

And remember, if the task you need a library for is relatively simple, it may be better to just write the implementation yourself. Again, squad committers can help you to make such determination.

## Code Reviews
------------------------

Code reviews are an important part of ensuring that a codebase is high-quality and bugs not detected by testing are minimized.
Code reviews even benefit the contributor, as it's an opportunity for the reviewer to share advice and expertise to help contributors gain programming experience.
Therefore, reviews are recommended whenever a pull request is large enough to need explanation, or if there is uncertainty.
The following are some questions that could be asked during a code review

* Does the change do what it needs to do?
    * Are there any introduced or exposed defects?
    * Are there unexpected side effects of the change?
    * Does the change depend on uncertain conditions? 

* If changing existing names or strings, were all references updated?
    * Text search tools like grep and rg are great for language-neutral ways to confirm all references were changed.

* Is duplication of code avoided?
    * Does the change reuse appropriate systems for the application and domain?

* Is the code easy to understand and maintain?
    * Is the functionality of the change clear when reading the code?
    * Are modules / classes / methods short and ready for future reuse?
    * Does the change improve maintainability of the code base?
    * Should this module / class be refactored to improve maintainability?

* Is the logging appropriate?
    * If the codebase has a formatted logger, is it being used appropriately?
    * Is the log verbosity correct?
    * Are errors being logged in a way that allows end-users or support to find a solution?
        * Do messages have message IDs that allow for documentation and lookup?
        * Do logs contain the variables involved in the error so users know what specifically is wrong?

* Is the change covered by the appropriate unit test?
    * Are all inputs / outputs checked for validity?
    * Are all data sources and variables checked for validity before use? 
    * Are there tests to prevent deadlock, timeouts, or data errors?
    * Are there implicit environmental expectations that may be changed? 
    * Are errors handled correctly?

* Version support
    * If web code, does it work on all supported browsers?
    * If server code, does this work on all supported OS? All supported dependencies (Java, nodeJS, etc)?

* Is there a change in the security model?
    * Is security maintained through appropriate used of access control, authentications, or data cleansing?
        * Never expose sensitive information. Sanitize logs of user input, and do not log sensitive variables
        * There should not be system information leaks when an error occurs in a network request, such as in HTTP error messages

* Has the performance changed?
    * How does this perform with many users?
    * Does this work with a large amount of data?

Keep in mind :)
Experience is what you get when you don't get what you want.
In theory there is no difference between theory and practice, but in practice there is.

## Codebase principles
------------------------

Our codebases have specific principles that the developers use in order to maintain quality, understandability, and ease of maintenance.




## Contact Us
------------------------

Get in touch using [Zowe Communication Channels](https://github.com/zowe/community/blob/master/README.md#communication-channels). You can find us in the `#zowe-dev` channel on Slack.
