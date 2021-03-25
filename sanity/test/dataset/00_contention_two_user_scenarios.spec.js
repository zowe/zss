const { v4:uuid}=require('uuid');
const {expect} = require('chai');
const {postDatasetEnqueue, deleteDatasetEnqueue, printError, getDatasetContents, postDatasetContents, postDatasetHeartbeat} =require('../../utils/api');

let optionsUser2, ZOWE_DATASET_TEST;
// https://github.com/zowe/zlux/issues/615
describe('00 contention two user scenarios', function() {
  before('verify environment variables', function() {
    // allow self signed certs
    process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';
    expect(process.env.ZOWE_USER2, 'ZOWE_USER2 is empty').to.not.be.empty;
    expect(process.env.ZOWE_PASSWD2, 'ZOWE_PASSWD2 is empty').to.not.be.empty;
    expect(process.env.ZOWE_DATASET_TEST, 'ZOWE_DATASET_TEST is empty').to.not.be.empty;
    ({ZOWE_DATASET_TEST} = process.env);
    optionsUser2={
      auth: {
        username: process.env.ZOWE_USER2,
        password: process.env.ZOWE_PASSWD2
      }
    };
  });

    // do not add test cases where you have to release lock
    describe('scenario 1 - lock held by user1', async () => {
      before('get lock user 1', async () => {
        await getDatasetContents(ZOWE_DATASET_TEST)
          .then((data)=>{
            expect(data).to.have.property('records');
          }).catch((err)=>{
            printError(err);
            throw new Error('check if dataset exists');
          });
        return postDatasetEnqueue(ZOWE_DATASET_TEST).catch((err)=>{
          printError(err);
          throw err;
        });
      });

      after('release lock user 1',  async () => {
        return deleteDatasetEnqueue(ZOWE_DATASET_TEST).catch((err)=>{
          printError(err);
          throw err;
        });
      });

      // sending heartbeat to keep lock alive
      beforeEach('send dataset heartbeat', async ()=>{
        return postDatasetHeartbeat().catch((err)=>{
          printError(err);
          throw err;
        });
      });
    
      it('try grab lock user 2, should fail', async ()=>{
        return postDatasetEnqueue(ZOWE_DATASET_TEST, optionsUser2)
          .then((data)=>{
            throw new Error('grab lock user 2 should fail but succeded');
          }).catch((err)=>{
            if(err.response) {
              let res = err.response;
              expect(res).to.have.property('status');
              expect(res.status).to.equal(409);
            }
          })
      });

      it('try read file user2 should succeed', async ()=>{
        return getDatasetContents(ZOWE_DATASET_TEST, optionsUser2)
          .then((data)=>{
            expect(data).to.have.property('records');
          }).catch((err)=>{
            printError(err);
            throw new Error('try read file user2 failed');
          });
      });

      it('try save file user2 should fail', async ()=>{
        let randStr = uuid().toString();
        await postDatasetContents(ZOWE_DATASET_TEST,  {records: [randStr]}, optionsUser2)
          .catch((err)=>{
            if(err.response) {
              let res = err.response;
              expect(res).to.have.property('status');
              expect(res.status).to.equal(500);
            }
          });
      
        return getDatasetContents(ZOWE_DATASET_TEST, optionsUser2).then((data) => {
          expect(data.toString()).to.not.have.string(randStr);
        });
      });

      it('try release lock from user2 held by user 1, should fail', async ()=>{
        return deleteDatasetEnqueue(ZOWE_DATASET_TEST, optionsUser2)
          .then((data)=> {
            throw new Error('should fail');
          })
          .catch((err)=>{
            if(err.response) {
              let res = err.response;
              expect(res).to.have.property('status');
              expect(res.status).to.equal(400);
            }
          });
      });


      it('save by user 1, user 2 should see latest changes', async ()=>{
        let randStr = uuid().toString();
        // save by user 1
        await postDatasetContents(ZOWE_DATASET_TEST, {records: [randStr]})
          .then((data)=>{
            expect(data).to.have.string('Updated dataset');
          }).catch((err)=>{
            printError(err);
            throw new Error('unable to update by user1');
          });

        // fetch by user 2 should see latest changes
        return getDatasetContents(ZOWE_DATASET_TEST, optionsUser2).then((data) => {
          expect(JSON.stringify(data)).to.have.string(randStr);
        }).catch((err)=>{
          printError(err);
          throw new Error('user2 could not find user1 changes');
        });
      });
    
    });

    // grab lock user 1, try save user 2, release lock user 1
    // dont add any tests in this decribe
    describe('scenario 2 - perform action by user 2 when lock held by user 1', async () => {

      before('get lock user 1', async () => {
        await getDatasetContents(ZOWE_DATASET_TEST)
          .then((data)=>{
          expect(data).to.have.property('records');
        }).catch((err)=>{
          printError(err);
          throw new Error('check if dataset exists');
        });
        return postDatasetEnqueue(ZOWE_DATASET_TEST).catch((err)=>{
          printError(err);
          throw err;
        });
      });

      it('try save file user1, user2 should receive when fetch dataset again', async ()=>{
        // try get content users 2 - lock held by user 1
        await getDatasetContents(ZOWE_DATASET_TEST, optionsUser2)
        .then((data)=>{
          expect(data).to.have.property('records');
        }).catch((err)=>{
          printError(err);
          throw new Error('try read file user2 failed');
        });
        
        let randStr = uuid().toString();
        // try save content user 2 - lock held by user1
        await postDatasetContents(ZOWE_DATASET_TEST,  {records: [randStr]}, optionsUser2)
          .catch((err)=>{
            if(err.response) {
              let res = err.response;
              expect(res).to.have.property('status');
              expect(res.status).to.equal(500);
            }
          });

        // fetch by user 2 to verify it did not save
        await getDatasetContents(ZOWE_DATASET_TEST, optionsUser2).then((data) => {
          expect(JSON.stringify(data)).to.not.have.string(randStr);
        });   

        // release lock user 1
        await deleteDatasetEnqueue(ZOWE_DATASET_TEST).catch((err)=>{
          printError(err);
          throw err;
        });

      randStr = uuid().toString();
      // try save again user 2
      await postDatasetContents(ZOWE_DATASET_TEST,  {records: [randStr]}, optionsUser2)
        .then((data)=>{
          expect(data).to.have.string('Updated dataset');
        })
        .catch((err)=>{
          printError(err);
          throw new Error('save after releasing lock from user 2 should succeed');
        });

      // fetch user 2 should see latest changes
      return getDatasetContents(ZOWE_DATASET_TEST, optionsUser2).then((data) => {
        expect(JSON.stringify(data)).to.have.string(randStr);
      });
    });
  });
});