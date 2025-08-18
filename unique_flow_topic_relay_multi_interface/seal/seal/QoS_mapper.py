
class QoSmapper(object):

    def __init__(self):
        self.QCI = None
        self.ExampleServices = None

    # based on 3GPP TS 23.203
    def getQCI(self, RT, PL, PDB, PELR, MDBV, DRAW):
        """Map QCI values from Resource Type (RT), Priority Level (PL), Packet Delay Budget (PDB), Packet Error Loss Rate (PELR), Maximum Data Burst Volume (MDBV), Data Rate Averaging Window (DRAW)"""
        print("mapper RT " + str(RT))
        print("mapper PL " + str(PL))
        print("mapper PDB " + str(PDB))
        print("mapper PELR " + str(PELR))
        hit = 0
        if RT == "GBR" and PDB <= 100 and PELR <= 10e-2: #and PL == 2
            self.QCI = 1
            self.ExampleServices = "Conversational Voice"
            hit = hit + 1
        if RT == "GBR" and PDB <= 150 and PELR <= 10e-3: #and PL == 4 
            self.QCI = 2
            self.ExampleServices = "Conversational Video (Live Streaming)"
            hit = hit + 1
        if RT == "GBR" and PDB <= 50 and PELR <= 10e-3: #and PL == 3 
            self.QCI = 3
            self.ExampleServices = "Real Time Gaming, V2X messages; Electricity distribution - medium voltage; Process automation - monitoring"
            hit = hit + 1
        if RT == "GBR" and PDB <= 300 and PELR <= 10e-6: #and PL == 5 
            self.QCI = 4
            self.ExampleServices = "Non-Conversational Video (Buffered Streaming)"
            hit = hit + 1
        if RT == "GBR" and PDB <= 75 and PELR <= 10e-2: #and PL == 0.7 
            self.QCI = 65
            self.ExampleServices = "Mission Critical user plane Push To Talk voice (e.g., MCPTT)"
            hit = hit + 1
        if RT == "GBR" and PDB <= 100 and PELR <= 10e-2: #and PL == 2 
            self.QCI = 66
            self.ExampleServices = "Non-Mission-Critical user plane Push To Talk voice"
            hit = hit + 1
        if RT == "GBR" and PDB <= 100 and PELR <= 10e-3: #and PL == 1.5
            self.QCI = 67
            self.ExampleServices = "Mission-Critical Video user plane"
            hit = hit + 1
        if RT == "GBR" and PDB <= 50 and PELR <= 10e-2: #and PL == 2.5 
            self.QCI = 75
            self.ExampleServices = "V2X messages"
            hit = hit + 1
        if RT == "GBR" and PDB <= 150 and PELR <= 10e-6: #and PL == 5.6
            self.QCI = 71
            self.ExampleServices = "Live Uplink Streaming"
            hit = hit + 1
        if RT == "GBR" and PDB <= 300 and PELR <= 10e-4: #and PL == 5.6
            self.QCI = 72
            self.ExampleServices = "Live Uplink Streaming"
            hit = hit + 1
        if RT == "GBR" and PDB <= 300 and PELR <= 10e-8: #and PL == 5.6
            self.QCI = 73
            self.ExampleServices = "Live Uplink Streaming"
            hit = hit + 1
        if RT == "GBR" and PDB <= 500 and PELR <= 10e-8: #and PL == 5.6
            self.QCI = 74
            self.ExampleServices = "Live Uplink Streaming"
            hit = hit + 1
        if RT == "GBR" and PDB <= 500 and PELR <= 10e-4: #and PL == 5.6
            self.QCI = 76
            self.ExampleServices = "Live Uplink Streaming"
            hit = hit + 1

        if RT == "Non-GBR" and PDB <= 100 and PELR <= 10e-6: #and PL == 1
            self.QCI = 5
            self.ExampleServices = "IMS Signalling"
            hit = hit + 1
        if RT == "Non-GBR" and PDB <= 300 and PELR <= 10e-6: #and PL == 6 
            self.QCI = 6
            self.ExampleServices = "Video (Buffered Streaming), TCP-based (www, email, chat, ftp, p2p file sharing, progressive video)"
            hit = hit + 1
        if RT == "Non-GBR" and PDB <= 100 and PELR <= 10e-3: #and PL == 7
            self.QCI = 7
            self.ExampleServices = "Voice, Video (Live Streaming), Interactive Gaming"
            hit = hit + 1
        if RT == "Non-GBR" and PDB <= 300 and PELR <= 10e-6: #and PL == 8
            self.QCI = 8
            self.ExampleServices = "Video (Buffered Streaming), TCP-based (e.g., www, email, chat, ftp, p2p file sharing, progressive video, etc.)"
            hit = hit + 1
        if RT == "Non-GBR" and PDB <= 300 and PELR <= 10e-6: #and PL == 9 
            self.QCI = 9
            self.ExampleServices = "Video (Buffered Streaming), TCP-based (e.g., www, email, chat, ftp, p2p file sharing, progressive video, etc.)"
            hit = hit + 1
        if RT == "Non-GBR" and PDB <= 1100 and PELR <= 10e-6: #and PL == 9
            self.QCI = 10
            self.ExampleServices = "Video (Buffered Streaming), TCP-based (e.g., www, email, chat, ftp, p2p file sharing, progressive video, etc.) and any service that can be used over sattelite access with these characteristics"
            hit = hit + 1
        if RT == "Non-GBR" and PDB <= 60 and PELR <= 10e-6: #and PL == 0.5
            self.QCI = 69
            self.ExampleServices = "Mission Critical delay sensitive signalling (e.g., MC-PTT signalling, MC video signalling)"
            hit = hit + 1
        if RT == "Non-GBR" and PDB <= 200 and PELR <= 10e-6: #and PL == 5.5
            self.QCI = 70
            self.ExampleServices = "Mission Critical Data (e.g., example services are the same as QCI 6/8/9)"
            hit = hit + 1
        if RT == "Non-GBR" and PDB <= 50 and PELR <= 10e-2: #and PL == 6.5
            self.QCI = 79
            self.ExampleServices = "V2X messages"
            hit = hit + 1
        if RT == "Non-GBR" and PDB <= 10 and PELR <= 10e-6: #and PL == 6.8
            self.QCI = 80
            self.ExampleServices = "Low latency eMBB applications (TCP/UDP-based); Augmented Reality"
            hit = hit + 1

        #PDB_limit = 10
        #MDBV_limit = 255
        # Table 6.1.7-B
        if RT == "GBR" and PDB <= 10 and PELR <= 10e-4 and MDBV[10] <= 255 and DRAW <= 2000: #and PL == 1.9
            self.QCI = 82
            self.ExampleServices = "Discrete Automation small packets"
            #PDB_limit = 10
            #MDBV_limit = 255
            hit = hit + 1
        if RT == "GBR" and PDB <= 10 and PELR <= 10e-4 and MDBV[10] <= 1354 and DRAW <= 2000: #and PL == 2.2 
            self.QCI = 83
            self.ExampleServices = "Discrete Automation big packets"
            #PDB_limit = 10
            #MDBV_limit = 1354
            hit = hit + 1
        if RT == "GBR" and PDB <= 30 and PELR <= 10e-5 and MDBV[30] <= 1354 and DRAW <= 2000: #and PL == 2.4
            self.QCI = 84
            self.ExampleServices = "Intelligent Transport Systems"
            #PDB_limit = 30
            #MDBV_limit = 1354
            hit = hit + 1
        if RT == "GBR" and PDB <= 5 and PELR <= 10e-5 and MDBV[5] <= 255 and DRAW <= 2000: #and PL == 2.1
            self.QCI = 85
            self.ExampleServices = "Electricity Distribution-high voltage"
            #PDB_limit = 30
            #MDBV_limit = 255
            hit = hit + 1
        #extra
        if PDB > 1000:
            self.QCI = 10
            self.ExampleServices = "Video (Buffered Streaming), TCP-based (e.g., www, email, chat, ftp, p2p file sharing, progressive video, etc.) and any service that can be used over sattelite access with these characteristics"
            hit = hit + 1

        print("QCI mapping done (hit#) " + str(hit) + ": <" + str(self.QCI) + ", " + str(self.ExampleServices) + ">")
        return (self.QCI, self.ExampleServices)
