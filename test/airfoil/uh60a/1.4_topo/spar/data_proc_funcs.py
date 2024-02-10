import numpy as np

def materialId2Name(params):
    mdb = {
        1: 'AS4 12k/E7K8_0.0054',
        2: 'T650-35 3k 976_fabric_0.0062',
        3: 'T650-35 12k/976_0.0052',
        4: 'E-Glass 7781/EA 9396_0.0083',
    }
    params['mn_st'] = mdb[params['mi_st']]
    params['mn_le'] = mdb[params['mi_le']]
    params['mn_te'] = mdb[params['mi_te']]


def calcLayupEndLocation(params):
    params['le_bottom_begin_1'] = 1 - params['le_top_end_1']
    params['le_bottom_begin_2'] = 1 - params['le_top_end_2']
    params['le_bottom_begin_3'] = 1 - params['le_top_end_3']
    params['le_bottom_begin_4'] = 1 - params['le_top_end_4']
    params['te_bottom_begin_1'] = 1 - params['te_top_end_1']
    params['te_bottom_begin_2'] = 1 - params['te_top_end_2']
    params['te_bottom_begin_3'] = 1 - params['te_top_end_3']
    params['te_bottom_begin_4'] = 1 - params['te_top_end_4']


def calcEmbeddedPoint(params):
    params['pfle1_x2'] = params['pnsmc_x2'] - params['nsmr'] - 0.005
    params['pfle2_x2'] = params['wl_x2'] + 0.005
    params['pfte1_x2'] = params['wt_x2'] - 0.01


def calcOffsetLE(json_args, params, bps, inter, *args, **kwargs):
    response_name = args[0]
    bp_name = args[1]
    rv = bps[bp_name]
    if response_name == 'sc2_le':
        rv = json_args['xo2_le'] - bps['xo2']
        bps['sc2_le'] = rv
    elif response_name == 'mc2_le':
        rv += bps['sc2_le']

    return rv


def calcMaxDiff(json_args, params, bps, inter, *args, **kwargs):
    # print(inter)
    response_name = args[0]
    rv = np.max(list(inter.values()))
    # diff = []
    # for v, t in zip(values, targets):
    #     diff.append(np.abs((v - t) / t))
    # return np.max(diff)
    inter[response_name] = rv
    return rv


def calcAbsRelDiff(json_args, params, bps, inter, *args, **kwargs):
    # print(args)
    response_name = args[0]
    bp_name = args[1]
    target = args[2]

    value = bps[bp_name]

    if response_name == 'sc2_le_err':
        # value = json_args['xo2_le'] - bps['xo2']
        # bps['sc2_le'] = json_args['xo2_le'] + value
        value += json_args['xo2_le']
    elif response_name == 'mc2_le_err':
        value += json_args['xo2_le']
        target += args[3]

    rv = np.abs((value - target) / target)
    # print(rv)
    inter[response_name] = rv

    return rv

