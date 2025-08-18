from urllib import request
from urllib import error
import json
from datetime import timedelta, date, datetime

from .NetworkExposureAPI import NetworkExposure_API

class SEAL(NetworkExposure_API):

    def __init__(self):
        self.deviceID = "11122233344455"
        self.valGroupId = None
        self.groupDocId = ""
        self.uniSubId = ""
        self.url_SEAL = 'http://example.com/'
        self.auth_token = None
        self.username = "foo"
        self.password = "bar"

        # if there is a SEAL server that provides responses, then this can be set to 0
        self.local_test = 1

        self.QCI = None
        self.uplinkMaxBitRate = None
        self.downlinkMaxBitRate = None
        self.dstIp = None
        self.dstPort = None
        self.srcPort = None
        self.protocol = None
        self.direction = None

        r = None

        print("SEAL instance created" + str(self))

    def Login(self):
        local_test = 1
        url = self.url_SEAL + "login"
        print("url: " + url)

        post_data = {'username': self.username,
                    "password": self.password 
                    }

        post_data = json.dumps(post_data)
        print(post_data)
        post_data = post_data.encode()

        if self.local_test == 0:
            try:
                #req = request.Request(url, data = post_data, method="POST")
                req = request.Request(url, method="POST")
                req.add_header('Content-Type', 'application/json')
                r = request.urlopen(req, data=post_data).read().decode('utf-8')
            except error.URLError as e:
                if hasattr(e, 'reason'):
                    print('We failed to reach a server.')
                    print('Reason: ', e.reason)
                elif hasattr(e, 'code'):
                    print('The server couldn\'t fulfill the request.')
                    print('Error code: ', e.code)
            else:
                # everything is fine
                # not used
                ret_code = 200  
        else:
            r = '''{"username":"foo","password":"bar","accessToken":"eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJ1c2VybmFtZSI6ImZvbyIsInBhc3N3b3JkIjoiYmFyIiwiaWF0IjoxNjY4MDg0NDI1fQ.lT4ABOQSHyJdIiF9rso06qcwrBkIxRFyolIgdBAI4l0"}'''

        data = json.loads(r)
        self.auth_token = data['accessToken']
        print("auth_token : " + self.auth_token)
        return 200

    def CreateDeviceGroup(self, grpDesc):
        local_test = 1
        url = self.url_SEAL + "ss-gm/v1/group-documents"
        print("url: " + url)

        post_data = {
            "valGroupId": 'ROS2-DOMAIN_ID-' + str(grpDesc),
            "grpDesc": 'ROS2-DOMAIN_ID-' + str(grpDesc),
            "members": [
                {
                "valUserId": self.deviceID,
                "valUeId": self.deviceID + "@valdomain.com"
                },
            ],
            "valGrpConf": "communicationType: IPV4",
            "valServiceIds": [
                "VAL-service-1"
            ],
            "suppFeat": "1"
        }

        post_data = json.dumps(post_data)
        print(post_data)
        post_data = post_data.encode()

        response = None
        response_data = None

        if self.local_test == 0:
            try:
                req = request.Request(url, method="POST")
                req.add_header('Content-Type', 'application/json')
                req.add_header('Authorization', 'Bearer ' + self.auth_token)
                response = request.urlopen(req, data=post_data)
                response_data = response.read().decode('utf-8')
            except error.URLError as e:
                if hasattr(e, 'reason'):
                    print('We failed to reach a server.')
                    print('Reason: ', e.reason)
                elif hasattr(e, 'code'):
                    print('The server couldn\'t fulfill the request.')
                    print('Error code: ', e.code)
            else:
                # everything is fine
                # not used
                ret_code = 200  
        else:
            response_data = '''{
                    "valGroupId": "string",
                    "grpDesc": "string",
                    "members": [
                        {
                        "valUserId": "string",
                        "valUeId": "string"
                        },
                        {
                        "valUserId": "string",
                        "valUeId": "string"
                        }
                    ],
                    "valGrpConf": "string",
                    "valServiceIds": [
                        "string"
                    ],
                    "valSvcInf": "string",
                    "suppFeat": "string",
                    "resUri": "https://example.com/ss-gm/v1/group-documents/96be8d4d422dd4f90eefd4ace1e4b8"
                    }'''

        try:
            self.groupDocId = response.getheader('Location') 
        except Exception as e:
            print(response_data)
            data = json.loads(response_data)
            self.groupDocId = data['resUri']
        print("groupDocId : " + str(self.groupDocId))
        #breakpoint()
        return 200

    def AddDeviceAsMemberToDeviceGroup(self, grpDesc):
        local_test = 1
        url = self.groupDocId
        print("url: " + url)

        post_data = {
            "valGroupId": 'ROS2-DOMAIN_ID-' + str(grpDesc),
            "grpDesc": 'ROS2-DOMAIN_ID-' + str(grpDesc),
            "members": [
                {
                "valUserId": self.deviceID,
                "valUeId": self.deviceID + "@valdomain.com"
                },
            ],
            "valGrpConf": "communicationType: IPV4",
            "valServiceIds": [
                "VAL-service-1"
            ],
            "suppFeat": "1"
        }

        post_data = json.dumps(post_data)
        print(post_data)
        post_data = post_data.encode()

        if self.local_test == 0:
            try:
                req = request.Request(url, method="PATCH")
                req.add_header('Content-Type', 'application/json')
                req.add_header('Authorization', 'Bearer ' + self.auth_token)
                r = request.urlopen(req, data=post_data).read().decode('utf-8')
            except error.URLError as e:
                if hasattr(e, 'reason'):
                    print('We failed to reach a server.')
                    print('Reason: ', e.reason)
                elif hasattr(e, 'code'):
                    print('The server couldn\'t fulfill the request.')
                    print('Error code: ', e.code)
            else:
                # everything is fine
                r = '{}'
                return 200;   
        else:
            r = '''{
                }'''
            return 200

        self.groupDocId = r.getheader('Location')
        print("groupDocId : " + str(self.groupDocId))
        #breakpoint()
        return 200
        

    def NetworkResourceAdaptation(self, GROUP_ID, QCI, uplinkMaxBitRate, downlinkMaxBitRate, dstIp, dstPort, srcPort, protocol, direction):
        local_test = 1
        url = self.url_SEAL + "ss-nra/v1/unicast-subscriptions"
        print("url: " + url)

        #request is valid for 7 days
        end_date = date.today() + timedelta(days=7)

        uniQoSReq_data = {
                    "type": "IPV4", 
                    "qci": QCI, 
                    "ulBW": uplinkMaxBitRate, 
                    "dlBW": downlinkMaxBitRate, 
                    "flowID": {
                        "dstIp": dstIp, 
                        "dstPort": dstPort, 
                        "srcPort": srcPort, 
                        "protocol": protocol, 
                        }
        }

        post_data =  {
                    "valTgtUe": {
                        "valUserId": self.deviceID,
                        "valUeId": self.deviceID + "@valdomain.com"
                    },
                    "uniQosReq": json.dumps(uniQoSReq_data),
                    "duration": end_date.strftime('%d/%m/%Y %H:%M:%S'),
                    "notifUri": "string",
                    "reqTestNotif": True,
                    "wsNotifCfg": {
                        "websocketUri": "string",
                        "requestWebsocketUri": True
                    },
                    "suppFeat": "string"
        }

        post_data = json.dumps(post_data)
        print(post_data)
        post_data = post_data.encode()

        if self.local_test == 0:
            try:
                req = request.Request(url, method="POST")
                req.add_header('Content-Type', 'application/json')
                req.add_header('Authorization', 'Bearer ' + self.auth_token)
                r = request.urlopen(req, data=post_data).read().decode('utf-8')
            except error.URLError as e:
                if hasattr(e, 'reason'):
                    print('We failed to reach a server.')
                    print('Reason: ', e.reason)
                elif hasattr(e, 'code'):
                    print('The server couldn\'t fulfill the request.')
                    print('Error code: ', e.code)
            else:
                # everything is fine
                r = '{}'
                return 200   
        else:
            r = '{}'
            return 200

        self.uniSubId = r.getheader('Location')
        print("uniSubId : " + str(self.uniSubId))
