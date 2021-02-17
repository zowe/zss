const _ = require('lodash');
const axios = require('axios');
const {expect} = require('chai');

process.env.NODE_TLS_REJECT_UNAUTHORIZED = '0';
expect(process.env.ZOWE_EXTERNAL_HOST, 'ZOWE_EXTERNAL_HOST is empty').to.not.be.empty;
expect(process.env.ZOWE_ZSS_PORT, 'ZOWE_ZSS_PORT is not defined').to.not.be.empty;
expect(process.env.ZOWE_USER, 'ZOWE_USER is not defined').to.not.be.empty;
expect(process.env.ZOWE_PASSWD, 'ZOWE_PASSWD is not defined').to.not.be.empty;
const { ZOWE_EXTERNAL_HOST, ZOWE_ZSS_PORT, ZOWE_USER, ZOWE_PASSWD } = process.env;

const baseURL = `http://${ZOWE_EXTERNAL_HOST}:${ZOWE_ZSS_PORT}`;

let timeout = 200000000;
let headers = { 'Content-Type': 'application/json' };
let auth = {
  username: ZOWE_USER,
  password: ZOWE_PASSWD
};
let maxRedirects = 0;

const REQ = axios.create({
  baseURL,
  timeout,
  headers,
  auth,
  maxRedirects
});

function printError(error) {
  if (error.response) {
    console.log('error.response');
    // The request was made and the server responded with a status code
    // that falls out of the range of 2xx
    console.log(error.response.data);
    console.log(error.response.status);
    console.log(error.response.headers);
  } else if (error.request) {
    // The request was made but no response was received
    // `error.request` is an instance of XMLHttpRequest in the browser and an instance of
    // http.ClientRequest in node.js
    console.log('error.request');
    console.log(error.request);
  } else {
    // Something happened in setting up the request that triggered an Error
    console.log('error.message');
    console.log('Error', error.message);
  }
  //console.log('error.config');
  //console.log(error.config);
}

async function getOMVS(options={}) {
  try {
    const res = await REQ.get('omvs', options);
    expect(res).to.have.property('data');
    return res.data;
  } catch (err) {
    throw err;
  }
}

async function getUnixfileContent(fullpath='',options={}) {
  let addOptions = {
    params: {responseType: 'raw'},
  };

  let merged = _.merge({}, addOptions, options);

  try {
    const res = await REQ.get(`/unixfile/contents${fullpath}`, merged);
    expect(res).to.have.property('data');
    return res.data;
  } catch (err) {
    throw err;
  }
}

async function getDatasetMetadata(searchPath='', options={}) {
  let addOptions = {
    params: {responseType: 'raw'},
  };

  let merged = _.merge({}, addOptions, options);

  try {
    const res = await REQ.get(`/datasetMetadata/name/${searchPath}`, merged);
    expect(res).to.have.property('data');
    return res.data;
  } catch (err) {
    throw err;
  }
}

async function getDatasetContents(datasetName, options={}) {
  let addOptions = {
    params: {responseType: 'raw'},
  };

  let merged = _.merge({}, addOptions, options);


  try {
    const res = await REQ.get(`/datasetContents/${datasetName}`, merged);
    expect(res).to.have.property('data');
    return res.data;
  } catch (err) {
    throw err;
  }
}

async function postDatasetContents(datasetName, datasetContent, options={}) {
  
  try {
    const res = await REQ.post(`/datasetContents/${datasetName}`, datasetContent, options);
    expect(res).to.have.property('data');
    return res.data;
  } catch (err) {
    throw err;
  }
}

async function getDatasetEnqueue(datasetName,options={}) {
  try {
    const res = await REQ.get(`/datasetEnqueue/${datasetName}`, );
    expect(res).to.have.property('data');
    return res.data;
  } catch (err) {
    throw err;
  }
}

async function deleteDatasetEnqueue(datasetName,options={}) {
  try {
    const res = await REQ.delete(`/datasetEnqueue/${datasetName}`);
    expect(res).to.have.property('data');
    return res.data;
  } catch (err) {
    throw err;
  }
}

async function getDatasetHeartbeat(options={}) {
  try {
    const res = await REQ.get('/datasetHeartbeat',options);
    expect(res).to.have.property('data');
    expect(res.data).to.be.empty;
    return '';
  } catch (err) {
    throw err;
  }
}

// Feel free to try out api, uncomment below comment 
// `node index.js`

// (async function() {
//   try {
//     console.log(await getDatasetContents('NAKUL.SAMPLE.JCL'));
//     console.log(await postDatasetContents('NAKUL.SAMPLE.JCL',{records: ['writing test']}));
//     console.log(await getDatasetContents('NAKUL.SAMPLE.JCL'));
//     // console.log(await postDatasetContents('NAKUL.SAMPLE.JCL', {records: ['TEST 500']}));
//     // console.log(await getDatasetContents('NAKUL.SAMPLE.JCL'));
//     // console.log(await deleteDatasetEnqueue('NAKUL.SAMPLE.JCL'));
//   } catch(err) {
//     console.log('handleError');
//     printError(err);
//   }
// }());

module.exports = {
  getDatasetHeartbeat,
  deleteDatasetEnqueue,
  getDatasetEnqueue,
  postDatasetContents,
  getDatasetContents,
  getDatasetMetadata,
  getUnixfileContent,
  getOMVS,
  printError
};