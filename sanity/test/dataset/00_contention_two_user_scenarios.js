import chai from 'chai';
import {getDatasetEnqueue, deleteDatasetEnqueue, printError, getDatasetContents, postDatasetContents, getDatasetHeartbeat} from '../utils/api';
import uuid from 'uuid';
const {expect} = chai;

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
  describe('lock held by user1', async () => {
    before('get lock user 1', async () => {
      getDatasetContents(ZOWE_DATASET_TEST)
        .then((data)=>{
          expect(data).to.have.property('records');
        }).catch((err)=>{
          printError(err);
          throw new Error('check if dataset exists');
        });
      getDatasetEnqueue(ZOWE_DATASET_TEST).catch((err)=>{
        printError(err);
        throw err;
      });
    });

    after('release lock user 1',  async () => {
      deleteDatasetEnqueue(ZOWE_DATASET_TEST).catch((err)=>{
        printError(err);
        throw err;
      });
    });

    // sending heartbeat to keep lock alive
    beforeEach('send dataset heartbeat', async ()=>{
      getDatasetHeartbeat();
    });
    
    it('try grab lock user 2, should fail', async ()=>{
      getDatasetEnqueue(ZOWE_DATASET_TEST, optionsUser2)
        .catch((err)=>{
          if(err.response) {
            let res = err.response;
            expect(res).to.have.property('status');
            expect(res.status).to.equal(409);
          }
        })
        .then(()=>{
          throw new Error('grab lock user 2 should fail but succeded');
        });
    });

    it('try read file user2 should succeed', async ()=>{
      getDatasetContents(ZOWE_DATASET_TEST, optionsUser2)
        .then((data)=>{
          expect(data).to.have.property('records');
        }).catch((err)=>{
          printError(err);
          throw new Error('try read file user2 failed');
        });
    });

    it('try save file user2 should fail', async ()=>{
      let randStr = uuid.v4().toString();
      postDatasetContents(ZOWE_DATASET_TEST,  {records: [randStr]}, optionsUser2)
        .catch((err)=>{
          if(err.response) {
            let res = err.response;
            expect(res).to.have.property('status');
            expect(res.status).to.equal(500);
          }
        });
      
      getDatasetContents(ZOWE_DATASET_TEST, optionsUser2).then((data) => {
        expect(data).to.not.have.string(randStr);
      });
    });

    it('try release lock from user2 held by user 1, should fail', async ()=>{
      deleteDatasetEnqueue(ZOWE_DATASET_TEST, optionsUser2)
        .catch((err)=>{
          if(err.response) {
            let res = err.response;
            expect(res).to.have.property('status');
            expect(res.status).to.equal(500);
          }
        });
    });


    it('try save file user1, user2 should receive when fetch dataset again', async ()=>{
      let randStr = uuid.v4().toString();
      // save by user 1
      postDatasetContents(ZOWE_DATASET_TEST, {records: [randStr]})
        .then((data)=>{
          expect(data).to.have.property('records');
        });

      // fetch by user 2 should see latest changes
      getDatasetContents(ZOWE_DATASET_TEST, optionsUser2).then((data) => {
        expect(data).to.have.string(randStr);
      }); 
    });
    
  });

  // grab lock user 1, try save user 2, release lock user 1
  // dont add any tests in this decribe
  describe('lock held by user1, then release then perform action by user 2', async () => {
    before('get lock user 1', async () => {
      getDatasetContents(ZOWE_DATASET_TEST)
        .then((data)=>{
          expect(data).to.have.property('records');
        }).catch((err)=>{
          printError(err);
          throw new Error('check if dataset exists');
        });
      getDatasetEnqueue(ZOWE_DATASET_TEST).catch((err)=>{
        printError(err);
        throw err;
      });
    });

    it('try save file user1, user2 should receive when fetch dataset again', async ()=>{
      let randStr = uuid.v4().toString();
      // try get content users 2 - lock held by user 1
      getDatasetContents(ZOWE_DATASET_TEST, optionsUser2)
        .then((data)=>{
          expect(data).to.have.property('records');
        }).catch((err)=>{
          printError(err);
          throw new Error('try read file user2 failed');
        });

      // try save content user 2 - lock held by user1
      postDatasetContents(ZOWE_DATASET_TEST,  {records: [randStr]}, optionsUser2)
        .catch((err)=>{
          if(err.response) {
            let res = err.response;
            expect(res).to.have.property('status');
            expect(res.status).to.equal(500);
          }
        });

      // release lock user 1
      deleteDatasetEnqueue(ZOWE_DATASET_TEST).catch((err)=>{
        printError(err);
        throw err;
      });

      randStr = uuid.v4().toString();
      // try save again user 2
      postDatasetContents(ZOWE_DATASET_TEST,  {records: [randStr]}, optionsUser2)
        .then((data)=>{
          expect(data).to.have.string(randStr);
        })
        .catch((err)=>{
          printError(err);
          throw new Error('save after releassing lock from user 2 should succeed');
        });
    });
  });
});