import msgpi.io.iovabs as miovabs

sg = miovabs.readVABSIn('ah64_sec1in.sg')

print('nodes:', len(sg.nodes))
print('elements:', len(sg.elements))

nodes_temp = sg.nodes
for e, ns in sg.elements.items():
  for n in ns:
    try:
      del nodes_temp[n]
    except KeyError:
      pass

print(nodes_temp)
