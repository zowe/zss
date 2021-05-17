# ZSS Sanity Test

Perform fast sanity test on a ZSS instance to see if service is running as expected.

Contents of this readme:

- [Programming Language And Main Testing Method](#programming-language-and-main-testing-method)
- [Run Test Cases On Your Local](#run-test-cases-on-your-local)
  * [Prepare NPM Packages](#prepare-npm-packages)
  * [Start Test](#start-test)


## Programming Language And Main Testing Method

- Node.js, with recommended [v8.x LTS](https://nodejs.org/docs/latest-v8.x/api/index.html)
- [Mocha](https://mochajs.org/)
- [Chai Assertion Library - BDD Expect/Should](https://www.chaijs.com/api/bdd/)

## Run Test Cases On Your Local

### Prepare NPM Packages

We have a dependency on NPM registry of Zowe Artifactory. You will need to configure npm registry to be `https://zowe.jfrog.io/zowe/api/npm/npm-release/`. This should have been handled by the `.npmrc` file in this folder.

With correct npm registry, you can run `npm install` to install all dependencies.


### Start Test

Example command:

```
export ZOWE_USER=user1 \
export ZOWE_PASSWD=psswd1 \
export ZOWE_ZSS_PORT=48549 \
export ZOWE_EXTERNAL_HOST=mainframe.com \
export ZOWE_USER2=user2 \
export ZOWE_PASSWD2=psswd2 \
export ZOWE_DATASET_TEST=USER.SAMPLE.JCL \
npm test
```

## Try API
```
cd test/utils/api
node index.js
```

## Try Single File - mocha scenarios
```
./node_modules/mocha/bin/_mocha test/dataset/00_contention_two_user_scenarios.js 
```

