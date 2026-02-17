import sys
import os
import datetime as dt
import math
import numpy as np
import xml.etree.ElementTree as et
import subprocess as sbp
import utilities as utl
import msmio as mio
import dakota.interfacing as di

file_name_base = sys.argv[1]
params, results = di.read_parameters_file(sys.argv[-2], sys.argv[-1])
evid = int(params.eval_id)
# format string
fse = ' x [{0:%H}:{0:%M}:{0:%S}] EVAL {1:d}: {2:s}'
fsi = ' > [{0:%H}:{0:%M}:{0:%S}] EVAL {1:d}: {2:s}'

# ====================================================================
# PreVABS

# template_name_main = project_name + '_pre.template'
# template_name_basepoints = project_name + '_pre_basepoints.template'
# template_name_baselines = project_name + '_pre_baselines.template'
# template_name_materials = project_name + '_pre_materialsDB.template'
# # template_name_layups = project_name + '_pre_layups.template'

# input_name_main = project_name + '_pre.xml'
# input_name_basepoints = project_name + '_pre_basepoints.dat'
# input_name_baselines = project_name + '_pre_baselines.xml'
# input_name_materials = project_name + '_pre_materialsDB.xml'
# # input_name_layups = project_name + '_pre_layups.xml'

# sbp.call(['perl', 'dprepro', sys.argv[1], template_name_main, input_name_main])
# sbp.call(['perl', 'dprepro', sys.argv[1], template_name_basepoints, input_name_basepoints])
# sbp.call(['perl', 'dprepro', sys.argv[1], template_name_baselines, input_name_baselines])
# sbp.call(['perl', 'dprepro', sys.argv[1], template_name_materials, input_name_materials])
# sbp.call(['perl', 'dprepro', sys.argv[1], template_name_layups, input_name_layups])

# input_name_main = '.\\' + input_name_main

# sbp.call(['prevabs', '-vabs', '-i', input_name_main, '-norun'])

# ====================================================================
# VABS Homogenization

template_name = file_name_base + '.dat.template'
input_name_vabs = file_name_base + '.dat'

print fsi.format(dt.datetime.now(), evid, 'Running dprepro...')
di.dprepro(template=template_name, parameters=params, output=input_name_vabs)
# sbp.call(['vabsiii', input_name_vabs])
print fsi.format(dt.datetime.now(), evid, 'Running VABS homogenization...')
FNULL = open(os.devnull, 'w')
sbp.call(
    ['vabsiii', input_name_vabs], 
    stdout=FNULL, stderr=sbp.STDOUT
)

print fsi.format(dt.datetime.now(), evid, 'Extracting results...')
# results = pss.getHomogenizationResultVABS(input_name_vabs)
simout = mio.readVABSOutHomo(input_name_vabs)
# mps = results['massperspan']
tsm = simout['tsm']
for i in range(6):
    for j in range(i, 6):
        label = 's' + str(i+1) + str(j+1)
        results[label].function = tsm[i][j]
# tfm = results['tfm']
# mass = results['mass']
# mc = results['mc']
# gc = results['gc']
# tc = results['tc']
# sc = results['sc']

# ac = [5.05, 0.0]

# d_acsc = utl.distance(ac, sc)
# d_acmc = utl.distance(ac, mc)

# rs14 = tsm[0][3]**2 / (tsm[0][0]*tsm[3][3]) * 100
# rs45 = tsm[3][4]**2 / (tsm[3][3]*tsm[4][4]) * 100
# rs46 = tsm[3][5]**2 / (tsm[3][3]*tsm[5][5]) * 100

# ====================================================================
# Write new params.in for GEBT

# with open('params_gebt.in', 'w') as fout:
#     fout.write('{0:16d} variables\n'.format(42))
#     for i in range(len(tfm)):
#         for j in range(i, len(tfm[i])):
#             label = 'c' + str(i+1) + str(j+1)
#             fout.write('{0:16.8E} {1:s}\n'.format(tfm[i][j], label))
#     for i in range(len(mass)):
#         for j in range(i, len(mass[i])):
#             label = 'm' + str(i+1) + str(j+1)
#             fout.write('{0:16.8E} {1:s}\n'.format(mass[i][j], label))

# ====================================================================
# GEBT

# template_name = project_name + '_eigen' + '_gebt.template'
# input_name_gebt = project_name + '_eigen' + '_gebt.dat'

# sbp.call(['perl', 'dprepro', 'params_gebt.in', template_name, input_name_gebt])
# sbp.call(['gebt', input_name_gebt])

# # inputs = pss.getInputGEBT(input_name_gebt)
# inputs = mio.readGEBTIn(input_name_gebt)
# if inputs['analysis'] == 0:
#     # ptr, mbr = pss.getResultGEBT(input_name_gebt, inputs)
#     ptr, mbr = mio.readGEBTOut(input_name_gebt, inputs)
# elif inputs['analysis'] == 3:
#     # eva, eve = pss.getEigenGEBT(input_name_gebt)
#     [eva, eve] = mio.readGEBTOutEigen(input_name_gebt)

# ====================================================================
# Prepare recover input file for VABS

# if inputs['analysis'] == 0:
#     mio.writeVABSInLocal(input_name_vabs, input_name_gebt)

# ====================================================================
# VABS localization

# sbp.call(['vabsiii', input_name_vabs])

# ====================================================================
# Write results.out

results.write()
