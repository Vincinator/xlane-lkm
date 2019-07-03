This is a brief introduction on the development process of SASSY itself. 

### Development Environement
This section provides a jump start for SASSY and Linux Kernel Development. 
We will setup the required tools and briefly explain the development process.

Edit your sources however you like. However, you can also use a ansible playbook
to bootstrap a working development enviroment with all required editing, testing and evaluation tools. 

The file can be found in this Repository: [SASSY-Eval](https://github.com/Distributed-Systems-Programming-Group/SASSY-Eval)


### Development Process
#### version control 
Master branch must always contain the latest compilable sources. 
The master branch can be used to evaluate and demonstrate the latest working SASSY version.

In order to achieve this simple goal, we use feature, bug and refactor branches. 
- feature branch: adds new functionality to SASSY (high priority)
- bug branch: ~~adds a new bug~~ fixes a existing bug (high priority)
- refactor branch: improve code quality (very low priority)
- doodle branch: try out stuff 

To open a branch, you can use the following command:
```
git checkout -b branch_type/descriptive_name_of_your_feature
```
Where branch_type is either "feature", "bug", "refactor" or "doodle". 

#### Develop
Do what you have to do. If possible, add simple tests.

#### Test
Use local compiler for a very basic "smoke test". 
Install the new version and run the existing tests (and your own).

#### Pull request
Push your changes to the Repo and create a pull request to merge with master branch.
Merge your changes if required. Test master branch.


### Specification Documents

The specification documents of each SASSY component provide a
brief description on how SASSY works (high level).
For low level details, please refer to the source code. 
The specification documents have the single purpose to give a 
brief introduction to the responibilities of that described SASSY component.

