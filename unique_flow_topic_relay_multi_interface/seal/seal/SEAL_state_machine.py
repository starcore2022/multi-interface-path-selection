from transitions import Machine
import random

from .SEAL import SEAL

class SEAL_state_machine(object):

    states = ['no_auth','no_group', 'group_created', 'connection_in_group', 'NRM_posted']

    def __init__(self, name, seal, ros2_domain_id):

        self.name = name
        self.seal = seal
        self.ros2_domain_id = ros2_domain_id

        self.machine = Machine(model=self, states=SEAL_state_machine.states, initial='no_auth')

        self.machine.add_transition(trigger='get_auth_token', source='no_auth', dest='no_group', conditions=['is_call_login'])
        self.machine.add_transition(trigger='create_group', source='no_group', dest='group_created', conditions=['is_call_group_creation'])
        self.machine.add_transition(trigger='add_connection_to_group', source='group_created', dest='connection_in_group', conditions=['is_call_AddDeviceAsMemberToDeviceGroup'])
        self.machine.add_transition(trigger='nrm_posted', source='connection_in_group', dest='NRM_posted', conditions=['is_call_NetworkResourceAdaptation'])

    def is_call_login(self):
        ok = 0
        ret = self.seal.Login()
        if (ret == 200):
            ok =1
        return ok

    def is_call_group_creation(self):
        ok = 0
        ret = self.seal.CreateDeviceGroup(self.ros2_domain_id)
        if (ret == 200):
            ok =1
        return ok

    def is_call_AddDeviceAsMemberToDeviceGroup(self):
        ok = 0
        ret = self.seal.AddDeviceAsMemberToDeviceGroup(self.ros2_domain_id)
        if (ret == 200):
            ok =1
        return ok

    def is_call_NetworkResourceAdaptation(self):
        ok = 0
        ok =1
        return ok
