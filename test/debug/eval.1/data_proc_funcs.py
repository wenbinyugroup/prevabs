import numpy as np

def materialId2Name(params):
    mdb = {
        1: 'AS4 12k/E7K8_0.0054',
        2: 'T650-35 3k 976_fabric_0.0062',
        3: 'T650-35 12k/976_0.0052',
        4: 'E-Glass 7781/EA 9396_0.0083',
    }
    params['mn_1'] = mdb[params['mi_1']]
    params['mn_2'] = mdb[params['mi_2']]
    params['mn_3'] = mdb[params['mi_3']]
    params['mn_4'] = mdb[params['mi_4']]
    params['mn_5'] = mdb[params['mi_5']]
    params['mn_6'] = mdb[params['mi_6']]
    params['mn_7'] = mdb[params['mi_7']]
    params['mn_8'] = mdb[params['mi_8']]
    params['mn_le_1'] = mdb[params['mi_le_1']]
    params['mn_le_2'] = mdb[params['mi_le_2']]
    params['mn_te_1'] = mdb[params['mi_te_1']]
    params['mn_te_2'] = mdb[params['mi_te_2']]


def calcMaxDiff(values, targets):
    diff = []
    for v, t in zip(values, targets):
        diff.append(np.abs((v - t) / t))
    return np.max(diff)


def calcAbsRelDiff(value, *args, **kwargs):
    return np.abs((value - args[0]) / args[0])

