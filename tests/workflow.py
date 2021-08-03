#
# py workflow demo
# @author Tobias Weber <tweber@ill.fr>
# @date 14-july-2021
# @license see 'LICENSE' file
#

import sys
import os
import math as m

cwd = os.getcwd()
sys.path.append(cwd)
#print(sys.path)

import taspaths as tas


# -----------------------------------------------------------------------------
# helper functions
# -----------------------------------------------------------------------------
def error(msg):
	print("Error: %s" % msg)
	exit(-1)

def warning(msg):
	print("Warning: %s" % msg)
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# options
# -----------------------------------------------------------------------------
write_pathmesh = False
write_path = False
show_plot = True
file_name = "../res/instrument.taspaths"
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# load instrument
# -----------------------------------------------------------------------------
print("Loading instrument definition...")

# create the instrument space and load an instrument definition
instrspace = tas.InstrumentSpace()
[file_ok, file_date] = tas.InstrumentSpace.load(file_name, instrspace)

if file_ok:
	print("Loaded \"%s\", dated %s." % (file_name, file_date))
else:
	error("Could not load \"%s\"." % (file_name))

print("Instrument definition loaded.\n")
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# set-up a sample single-crystal
# -----------------------------------------------------------------------------
tascalc = tas.TasCalculator()
tascalc.SetScatteringSenses(True, False, True)
tascalc.SetSampleLatticeConstants(5, 5, 5)
tascalc.SetSampleLatticeAngles(90, 90, 90, True)
tascalc.UpdateB()
tascalc.UpdateUB()
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# build path mesh
# -----------------------------------------------------------------------------
print("Building path mesh...")

# set instrument scattering senses
#mem = tas.MemManager()
#senses = mem.NewRealArray(3)
#mem.SetRealArray(senses, 0, 1)
#mem.SetRealArray(senses, 1, -1)
#mem.SetRealArray(senses, 2, 1)

# create the paths builder object
builder = tas.PathsBuilder()
builder.AddConsoleProgressHandler()
builder.SetInstrumentSpace(instrspace)
#builder.SetScatteringSenses(senses)
builder.SetTasCalculator(tascalc)
print("Path builder uses %d threads." % builder.GetMaxNumThreads())

# angular ranges to probe
angle_padding = 4.
a2_delta = 1./180.*m.pi
a4_delta = 2./180.*m.pi
a2_begin = 0. - angle_padding*a2_delta
a2_end = m.pi + angle_padding*a2_delta
a4_begin = -m.pi - angle_padding*a4_delta
a4_end = m.pi + angle_padding*a4_delta

if not builder.CalculateConfigSpace(
	a2_delta, a4_delta,
	a2_begin, a2_end,
	a4_begin, a4_end):
	error("Angular configuration space could not be calculated.")

if not builder.CalculateWallContours(True, False):
	error("Obstacle contours could not be calculated.")

if not builder.CalculateLineSegments():
	error("Line segments could not be calculated.")

if not builder.CalculateVoronoi(False):
	error("Voronoi diagram could not be calculated.")

print("Finished building path mesh.\n")
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# set-up the start and target coordinates of a path
# -----------------------------------------------------------------------------
print("Calculating path...")

tascalc.SetKf(1.4)
start_angles = tascalc.GetAngles(0.5, 0., 0., 1.)
target_angles = tascalc.GetAngles(1.5, -0.5, 0., 2.5)

# take absolute angles
start_angles.monoXtalAngle = abs(start_angles.monoXtalAngle)
start_angles.sampleXtalAngle = abs(start_angles.sampleXtalAngle)
start_angles.sampleScatteringAngle = abs(start_angles.sampleScatteringAngle)
target_angles.monoXtalAngle = abs(target_angles.monoXtalAngle)
target_angles.sampleXtalAngle = abs(target_angles.sampleXtalAngle)
target_angles.sampleScatteringAngle = abs(target_angles.sampleScatteringAngle)

print("Start angles: a1 = %.2f deg, a5 = %.2f deg, a3 = %.2f deg, a4 = %.2f deg." % (
	start_angles.monoXtalAngle / m.pi*180.,
	start_angles.anaXtalAngle / m.pi*180.,
	start_angles.sampleXtalAngle / m.pi*180.,
	start_angles.sampleScatteringAngle / m.pi*180.))

print("Target angles: a1 = %.2f deg, a5 = %.2f deg, a3 = %.2f deg, a4 = %.2f deg." % (
	target_angles.monoXtalAngle / m.pi*180.,
	target_angles.anaXtalAngle / m.pi*180.,
	target_angles.sampleXtalAngle / m.pi*180.,
	target_angles.sampleScatteringAngle / m.pi*180.))
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# find path
# -----------------------------------------------------------------------------
#path = builder.FindPath(40./180.*m.pi, -80./180.*m.pi, 120./180.*m.pi, 105./180.*m.pi)

path = builder.FindPath(
		start_angles.monoXtalAngle * 2., start_angles.sampleScatteringAngle,
		target_angles.monoXtalAngle * 2., target_angles.sampleScatteringAngle)
if not path.ok:
	error("No path could be found.")

builder.SetSubdivisionLength(0.5)
vertices = builder.GetPathVerticesAsPairs(path, True, True)

print("Finished calculating path.\n")
# -----------------------------------------------------------------------------


# -----------------------------------------------------------------------------
# output
# -----------------------------------------------------------------------------
# write the path mesh vertices to a file
if write_pathmesh:
	if not builder.SaveToLinesTool("lines.xml"):
		warning("Could not save line segment diagram.")

# write the path vertices to a file
if write_path:
	with open("path.dat", "w") as datafile:
		for vertex in vertices:
			datafile.write("%.4f %.4f\n" % (vertex[0], vertex[1]))

# plot the angular configuration space
if show_plot:
	import matplotlib.pyplot as plt

	# plot obstacles
	numgroups = builder.GetNumberOfLineSegmentRegions()
	print("Number of regions: %d." % numgroups)
	for regionidx in range(numgroups):
		region = builder.GetLineSegmentRegionAsArray(regionidx)
		x1, y1, x2, y2 = zip(*region)
		plt.fill(x1, y1, "-", linewidth = 1.,
			fill = not builder.IsRegionInverted(regionidx),
			color = "#ff0000")

	# plot path
	x, y = zip(*vertices)
	plt.xlabel("Sample Scattering Angle 2\u03b8_S (deg)")
	plt.ylabel("Monochromator Scattering Angle 2\u03b8_M (deg)")
	plt.plot(x, y, "-", linewidth=2)

	plt.show()
# -----------------------------------------------------------------------------
