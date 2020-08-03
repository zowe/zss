# 
# This program and the accompanying materials are
# made available under the terms of the Eclipse Public License v2.0 which accompanies
# this distribution, and is available at https://www.eclipse.org/legal/epl-v20.html
#
# SPDX-License-Identifier: EPL-2.0
#
# Copyright Contributors to the Zowe Project.
#

from flask import Flask
from flask import request
import base64
from flask import make_response

app = Flask(__name__)

###  TODO:
#RBAC routes for permissions
#Permissions changed to array [7,7,7] to better represent all the different types of permissoins and add permission check function

global_username = "mock"
global_password = "pass"
global_password_expired = True

global_datasets = [
    {
        "name":"MOCK.BRODCAST",
        "csiEntryType":"A",
        "volser":"MIGRAT",
        "recfm": {
            "recordLength":"F",
            "isBlocked":True,
            "carriageControl":"unknown"
        },
        "dsorg": {
            "organization":"sequential",
            "maxRecordLen":150,
            "totalBlockSize":1500
        }
    },
    {
        "name":"MOCK.JCLLIB.TEST",
        "csiEntryType":"A",
        "volser":"TSP121",
        "recfm": {
            "recordLength":"F",
            "isBlocked":True,
            "carriageControl":"unknown"
        },
        "dsorg": {
            "organization":"partitioned",
            "isPDSDir":True,
            "isPDSE":True,
            "maxRecordLen":80,
            "totalBlockSize":32720
        },
        "members": [
            {
                "name":"BIGVSAM",
                "records": ["test", "test"]
            },
            {
                "name":"NEWESDS"
            },
            {
                "name":"NEWKSDS"
            },
            {
                "name":"VSAMCOPY"
            }
        ]
    },
    {
        "name":"MOCK.VSAM5",
        "csiEntryType":"C",
        "records": ["vsam", "test"]
    }
]

volserId = 1
def genVolserId():
    global volserId
    volserId = volserId + 1
    return "TSP" + str(volserId)

global_directory = {
    "permissions": 777,
    "type": "folder",
    "contents": {
        "folder1": {
            "permissions": 777,
            "type": "folder",
            "contents": {
                "file1": {
                    "permissions": 777,
                    "type": "file",
                    "contents": {
                        "raw": "This is file1",
                        "b64": "VGhpcyBpcyBmaWxlMQ=="
                    }
                },
                "folderA": {
                    "permissions": 777,
                    "type": "folder",
                    "contents": {
                        "fileA": {
                            "permissions": 777,
                            "type": "file",
                            "contents": {
                                "raw": "This is fileA",
                                "b64": "VGhpcyBpcyBmaWxlQQ=="
                            }
                        }
                    }
                }
            },
        },
        "folder2": {
            "permissions": 777,
            "type": "folder",
            "contents": {}
        },
        "file1": {
            "permissions": 777,
            "type": "file",
            "contents": {
                "raw": "This is file1",
                "b64": "VGhpcyBpcyBmaWxlMQ=="
            }
        },
        "file2": {
            "permissions": 777,
            "type": "file",
            "contents": {
                "raw": "Goodbye",
                "b64": "VGhpcyBpcyBmaWxlMg=="
            }
        },
        "noPermissions": {
            "permissions": 0,
            "type": "file",
            "contents": {
                "raw": "You should not be able to read this",
                "b64": "WW91IHNob3VsZCBub3QgYmUgYWJsZSB0byByZWFkIHRoaXM="
            }
        }
    }
}
@app.route('/login/', methods=['POST'])
@app.route('/login', methods=['POST'])
def login():
    if request.get_json()['username'] == global_username:
        if request.get_json()['password'] == global_password:
            if global_password_expired == False:
                resp = make_response("Login Successful")
                resp.set_cookie("jedHTTPSession", "xrQdqvc2J7WWlew2OXU1RtwwUXXD9KGJ35x5IZTrSrX18y80OVBI8A")
                return resp
            else:
                return "Password Expired", 428
    return "Incorrect Login Info", 400

@app.route('/password/', methods=['POST'])
@app.route('/password', methods=['POST'])
def change_password():
    global global_password
    global global_password_expired
    if global_password == request.get_json()['password'] and global_username == request.get_json()['username']:
        global_password = request.get_json()['newPassword']
        global_password_expired= False
        return {"status":"Password Succesfully Changed"}
    else:
        return {"status": "Username or password is incorrect. Please try again."}

@app.route('/unixfile/contents/', methods=['GET'], defaults={'subpath': ''}, strict_slashes=False)
@app.route('/unixfile/contents/<path:subpath>', methods=['PUT', 'GET', 'DELETE'], strict_slashes=False)
def unix_contents(subpath):
    if request.method == 'GET':
        responseType = request.args.get("responseType")
        if responseType is None:
            responseType = "b64"
        test = {'entries':[]}
        if subpath == '':
            for key in global_directory['contents'].keys():
                if global_directory['contents'][key]['contents'] is not None:
                    test['entries'].append(
                        {
                            "name": key,
                            "path": "/" + key,
                            "directory": True if global_directory['contents'][key]['type'] == "folder" else False,
                            "size": 9,
                            "ccsid": 0,
                            "createdAt": "2018-11-03T14:18:27",
                            "mode": 777
                        }
                    )
        else:
            paths = subpath.split('/')
            directory = global_directory
            for path in paths:
                directory = directory["contents"][path]
            if directory['type'] == 'file':
                if directory['permissions'] == 777:
                    return directory['contents'][responseType]
                else:
                    return {"error": "Forbidden"}, 403
            else:
                for key in directory['contents'].keys():
                    if directory['contents'][key]['contents'] is not None:
                        test['entries'].append(
                            {
                                "name": key,
                                "path": "/" + subpath + '/' + key,
                                "directory": True if directory['contents'][key]['type'] == "folder" else False,
                                "size": 9,
                                "ccsid": 0,
                                "createdAt": "2018-11-03T14:18:27",
                                "mode": 777
                            }
                        )
        return test
    elif request.method == 'PUT':
        paths = subpath.split('/')
        directory = global_directory
        for i in range(len(paths)):
            if paths[i] in directory['contents'].keys():
                directory = directory["contents"][paths[i]]
            else:
                if i == len(paths)-1:
                    directory['contents'][paths[i]] = {
                        "type": "file",
                        "contents": {}
                    }
                else:
                    directory['contents'][paths[i]] = {
                        "type": "folder",
                        "contents": {}
                    }
                directory = directory["contents"][paths[i]]
        directory['contents']['b64'] = request.get_data().decode('utf-8')
        directory['contents']['raw'] = base64.b64decode(request.get_data().decode('utf-8'))
        return {"msg":"Successfully wrote file."}
    elif request.method == 'DELETE':
        paths = subpath.split('/')
        directory = global_directory
        for path in paths:
            directory = directory["contents"][path]
        directory['contents'] = None
        return {'msg': "Successfully deleted a file"}

@app.route('/unixfile/mkdir/<path:subpath>', methods=['POST'])
def unix_mkdir(subpath):
    if request.method == 'POST':
        paths = subpath.split('/')
        directory = global_directory
        for path in paths:
            if path in directory['contents'].keys():
                directory = directory["contents"][path]
            else:
                directory['contents'][path] = {
                    "type": "folder",
                    "contents": {}
                }
        return {"msg": "Successfully created directory: /" + subpath}

@app.route('/datasetMetadata/name/', methods=['GET'], defaults={'dataset': ''})
@app.route('/datasetMetadata/name/<path:dataset>', methods=['GET', 'POST'])
def dataset_metadata(dataset):
    if dataset == "":
        return "No dataset name provided"
    if request.args.get("addQualifiers") == "true":
        if dataset.count("*") < 3:
            while dataset.endswith('*') or dataset.endswith('.'):
                dataset = dataset[:-1]
            path = dataset.replace('(', ".").replace(')', "").split(".")
        else:
            return {
                "_objectType": "com.rs.mvd.base.dataset.metadata",
                "_metadataVersion": "1.1",
                "hasMore": 0,
                "datasets": [],
            }
        dataset_list = []
        for data in global_datasets:
            match = True
            test = data['name'].split('.')
            for i in range(len(path)):
                try:
                    if path[i] != test[i]:
                        match = False
                except:
                    match = False
            if match:
                dataset_list.append(data)
        return {
            "_objectType":"com.rs.mvd.base.dataset.metadata",
            "_metadataVersion":"1.1",
            "hasMore":0,
            "datasets": dataset_list,
        }
    else:
        wildcardCount = 0
        if dataset.count("*") < 3:
            while dataset.endswith('*') or dataset.endswith('.'):
                if dataset.endswith('*'):
                    wildcardCount = wildcardCount + 1
                dataset = dataset[:-1]
            path = dataset.replace('(', ".").replace(')', "").split(".")
        else:
            return {
                "_objectType": "com.rs.mvd.base.dataset.metadata",
                "_metadataVersion": "1.1",
                "hasMore": 0,
                "datasets": [],
            }
        dataset_list = []
        if wildcardCount != 0:
            for data in global_datasets:
                dataset_name = data['name'].split('.')
                match = False
                if (len(dataset_name) == len(path)+wildcardCount) or wildcardCount==2:
                    match = True
                    for i in range(len(path)):
                        try:
                            if path[i] != dataset_name[i]:
                                match = False
                        except:
                            match = False
                if match:
                    dataset_list.append(data)
            return {
                "_objectType": "com.rs.mvd.base.dataset.metadata",
                "_metadataVersion": "1.1",
                "hasMore": 0,
                "datasets": dataset_list,
            }
        else:
            return {
                "_objectType": "com.rs.mvd.base.dataset.metadata",
                "_metadataVersion": "1.1",
                "hasMore": 0,
                "datasets": [x for x in global_datasets if x['name'] == dataset],
            }

@app.route('/datasetContents/', methods=['GET'], defaults={'dataset': ''})
@app.route('/datasetContents/<path:dataset>', methods=['GET', 'POST', 'DELETE'])
def dataset_contents(dataset):
    global global_datasets
    if request.method == 'GET':
        if dataset.endswith(')'):
            dataset_names = dataset.replace(")","").split("(")
            for data in global_datasets:
                if dataset_names[0] == data['name']:
                    return {
                        "records" : x.get('records', []) for x in data['members'] if x['name'] == dataset_names[1]
                    }
        else:
            for data in global_datasets:
                if dataset == data['name']:
                    if data['volser'] == "MIGRAT":
                        data['volser'] = genVolserId()
            return {
                "records": x.get('records', []) for x in global_datasets if x['name'] == dataset
            }
    elif request.method == 'POST':
        if dataset.endswith(')'):
            dataset_names = dataset.replace(")","").split("(")
            for data in global_datasets:
                if dataset_names[0] == data['name']:
                    for member in data['name']['members']:
                        if dataset_names[1] == member['name']:
                            member['records'] = request.get_data()['records']
    elif request.method == 'DELETE':
        if dataset.endswith(')'):
            dataset_names = dataset.replace(")","").split("(")
            for data in global_datasets:
                if dataset_names[0] == data['name']:
                    data['members'] = ([x for x in data['members'] if x['name'] != dataset_names[1]])
                    return {"msg":"Data set member " + dataset + " was deleted successfully"}
        else:
            global_datasets = [x for x in global_datasets if x['name'] != dataset]
            return {"msg": "Data set " + dataset + " was deleted successfully"}

@app.route('/VSAMdatasetContents/<path:dataset>', methods=['DELETE', 'POST'])
def VSAMdataset_contents(dataset):
    global global_datasets
    if request.method == 'GET':
        for data in global_datasets:
            if dataset == data['name']:
                if data['volser'] == "MIGRAT":
                    data['volser'] = genVolserId()
        return {
            "records": x.get('records', []) for x in global_datasets if x['name'] == dataset
        }
    elif request.method == 'DELETE':
        global_datasets= [x for x in global_datasets if x['name'] != dataset]
        return {"msg":"VSAM dataset " + dataset + " was successfully deleted"}
    elif request.method == 'POST':
        for data in global_datasets:
            if dataset == data['name']:
                data['records'] = request.get_json()['records']


@app.route('/​datasetMetadata​/hlq', methods=['GET'])
def dataset_metadata_hlq():
    global global_datasets
    types = None
    try:
        types = request.args.get('types')
    except:
        types = None
    if types is not None:
        return {
            "_objectType": "com.rs.mvd.base.dataset.metadata",
            "_metadataVersion": "1.1",
            "hasMore": 0,
            "datasets": [x for x in global_datasets if x['csiEntryType'] == types]
        }
    else:
        return {
            "_objectType": "com.rs.mvd.base.dataset.metadata",
            "_metadataVersion": "1.1",
            "hasMore": 0,
            "datasets": global_datasets
        }

@app.route('/unixfile/touch/<path:subpath>', methods=['POST'])
def unixfile_touch_file(subpath):
    if request.method == 'POST':
        paths = subpath.split('/')
        directory = global_directory
        for i in range(len(paths)):
            if paths[i] in directory['contents'].keys():
                directory = directory["contents"][paths[i]]
            else:
                if i == len(paths)-1:
                    directory['contents'][paths[i]] = {
                        "type": "file",
                        "permissions": 777,
                        "contents": {}
                    }
                else:
                    directory['contents'][paths[i]] = {
                        "type": "folder",
                        "contents": {},
                        "permissions": 777
                    }
                directory = directory["contents"][paths[i]]
        if 'b64' not in directory['contents'].keys():
            directory['contents']['b64'] = ""
            directory['contents']['raw'] = ""
            return {"msg":"Successfully wrote file."}
        else:
            return {'error': 'File already exists'}, 400

@app.route('/unixfile/rename/<path:subpath>', methods=['POST'])
def unixfile_rename(subpath):
    if request.method == 'POST':
        newNames = request.get_json()['newName'].split('/')
        paths = subpath.split('/')
        directory = global_directory
        for i in range(len(paths)-1):
            directory = directory["contents"][paths[i]]
        newDirectory  = directory['contents'].pop(paths[len(paths)-1])
        dir = global_directory
        for i in range(len(newNames)):
            if i == len(newNames)-1:
                dir['contents'][newNames[i]] = newDirectory
            dir = dir["contents"][newNames[i]]
        return {"msg": "File Successfully Renamed"}


@app.route('/unixfile/copy/<path:subpath>', methods=['POST'])
def unixfile_copy(subpath):
    if request.method == 'POST':
        newNames = request.get_json()['newName'].split('/')
        paths = subpath.split('/')
        directory = global_directory
        for i in range(len(paths)-1):
            directory = directory["contents"][paths[i]]
        newDirectory  = directory['contents'][paths[len(paths)-1]].copy()
        dir = global_directory
        for i in range(len(newNames)):
            if i == len(newNames)-1:
                dir['contents'][newNames[i]] = newDirectory
            dir = dir["contents"][newNames[i]]
        return {"msg": "File Successfully Copied"}

if __name__ == '__main__':
    app.run(debug=True)

