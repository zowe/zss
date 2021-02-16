import chai from 'chai';
import {getDatasetEnqueue, deleteDatasetEnqueue, printError, getDatasetContents} from '../utils/api';
const {expect} = chai;

var sleep = timeout => {
  return new Promise(resolve => {
    setTimeout(() => {
      console.log(`I was sleeping ${timeout} ms`);
      resolve();
    }, timeout);
  });
};

let optionsUser2, ZOWE_DATASET_TEST;
// 120 seconds
let WAIT__BEFORE_HEARTBEAT = 120000;

// https://github.com/zowe/zlux/issues/616
describe('01 client crash scenario', function() {
  before('verify environment variables', function() {
    process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';
    expect(process.env.ZOWE_DATASET_TEST, 'ZOWE_DATASET_TEST is empty').to.not.be.empty;
    ({ZOWE_DATASET_TEST} = process.env);
    optionsUser2={
      auth: {
        username: process.env.ZOWE_USER2,
        password: process.env.ZOWE_PASSWD2
      }
    };
  });

  describe('grab lock wait for 2 mins, and release lock should fail, get lock should succeed ', async () => {
    this.timeout(WAIT__BEFORE_HEARTBEAT*5);

    before('grab dataset lock, but dont send heartbeat', async ()=>{
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

      //simulating client crash with sleep
      await sleep(WAIT__BEFORE_HEARTBEAT);
    });

    it('delete lock should fail, lock must be released on receiving no heartbeat', () => {
      deleteDatasetEnqueue(ZOWE_DATASET_TEST).catch((err)=>{
        if(err.response) {
          let res = err.response;
          expect(res).to.have.property('status');
          expect(res.status).to.equal(500);
        }
      }).then(()=>{
        throw new Error(`lock should not exist after ${WAIT__BEFORE_HEARTBEAT/1000} seconds, but exists`);
      });
    });

    it('get lock should succeed for user 2, user 1 lock must be released on receiving no heartbeat', () => {
      getDatasetEnqueue(ZOWE_DATASET_TEST, optionsUser2).catch((err)=>{
        printError(err);
        throw err;
      });
    });
  });
});