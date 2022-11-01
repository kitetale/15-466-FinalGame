#!/usr/bin/env python

#based on 'export-sprites.py' and 'glsprite.py' from TCHOW Rainbow; code used is released into the public domain.

import sys
import bpy
import struct
import bmesh
import mathutils
import re;

args = []
for i in range(0,len(sys.argv)):
	if sys.argv[i] == '--':
		args = sys.argv[i+1:]

if len(args) != 4:
	print("\n\nUsage:\nblender --background --python export-bone-animations.py -- <infile.blend> <object> <action[;action2][;...]> <outfile.character>\nExports an armature-animated mesh to a binary blob.\n<action> can also be a named frame range as per '[100,150]Walk'\n<action> can specify root transforms by appending 'Walk!local' (local to armature),'Walk!global' (world-relative),'Walk!first' (first-frame relative)")
	exit(1)

infile = args[0]
object_name = args[1]
action_names = args[2].split(";")
outfile = args[3]

print("Will read '" + infile + "' and export object '" + object_name + "' with actions '" + "', '".join(action_names) + "'")

#TODO: read params file(?)
#object_name = "Blob"
#action_names = ["Stand", "Run"]

bpy.ops.wm.open_mainfile(filepath=infile)

#Find the object and its armature:
obj = bpy.data.objects[object_name]

objs = []

if obj.type == 'ARMATURE':
	print(object_name + " is an armature; exporting all objects that reference it:")
	armature = obj
	for o in bpy.data.objects:
		if o.type == 'MESH' and o.find_armature() == armature:
			print("  '" + o.name + "'")
			objs.append(o)
elif obj.type == 'MESH':
	objs.append(obj)
	armature = obj.find_armature()
else:
	print("Unknown object type '" + obj.type + "'")

#set everything visible:
def set_visible(layer_collection):
	layer_collection.exclude = False
	layer_collection.hide_viewport = False
	layer_collection.collection.hide_viewport = False
	for child in layer_collection.children:
		set_visible(child)

set_visible(bpy.context.view_layer.layer_collection)

#bpy.context.scene.layers = armature.layers
armature.hide_viewport = False
for obj in objs:
	obj.hide_viewport = False

#-----------------------------
#write out appropriate data:

strings_data = b''
#write_string will add a string to the strings section and return a packed (begin,end) reference:
def write_string(string):
	global strings_data
	begin = len(strings_data)
	strings_data += bytes(string, 'utf8')
	end = len(strings_data)
	return struct.pack('II', begin, end)


bone_data = b''

bone_name_to_idx = dict()

#write bind pose info for bone, return packed index:
def write_bone(bone):
	global bone_data

	if bone == None: return struct.pack('i', -1)
	if bone.name in bone_name_to_idx: return bone_name_to_idx[bone.name]
	#bone will be stored as:
	bone_data += write_string(bone.name) #name (begin,end)
	bone_data += write_bone(bone.parent) #parent (index, or -1)

	#bind matrix inverse as 3-row, 4-column matrix:
	transform = bone.matrix_local.copy()
	transform.invert()
	#Note: store *column-major*:
	bone_data += struct.pack('3f', transform[0].x, transform[1].x, transform[2].x)
	bone_data += struct.pack('3f', transform[0].y, transform[1].y, transform[2].y)
	bone_data += struct.pack('3f', transform[0].z, transform[1].z, transform[2].z)
	bone_data += struct.pack('3f', transform[0].w, transform[1].w, transform[2].w)

	#Could just store bind pose as TRS, or even parent-relative TRS:
	##bind matrix as TRS:
	#bone_data += struct.pack('3f', transform[0].x, transform[0].y, transform[0].z)
	#bone_data += struct.pack('4f', transform[1].x, transform[1].y, transform[1].z, transform[1].w)
	#bone_data += struct.pack('3f', transform[2].x, transform[2].y, transform[2].z)
	#Could store bone length for IK purposes

	#finally, record bone index:
	bone_name_to_idx[bone.name] = struct.pack('i', len(bone_name_to_idx))


frame_data = b''
frame_count = 0

#write frame:
def write_frame(pose, root_xf=mathutils.Matrix()):
	global frame_data
	global frame_count
	frame_count += 1

	for name in idx_to_bone_name:
		##Store pose as transform matrix:
		#frame_data += struct.pack('4f', *pose[name].matrix[0])
		#frame_data += struct.pack('4f', *pose[name].matrix[1])
		#frame_data += struct.pack('4f', *pose[name].matrix[2])
		#Store pose as parent-relative TRS: (better for interpolation)
		pose_bone = pose.bones[name]
		if pose_bone.parent:
			to_parent = pose_bone.parent.matrix.copy()
			to_parent.invert()
			local_to_parent = to_parent @ pose_bone.matrix
		else:
			local_to_parent = root_xf @ pose_bone.matrix

		trs = local_to_parent.decompose()
		frame_data += struct.pack('3f', trs[0].x, trs[0].y, trs[0].z)
		frame_data += struct.pack('4f', trs[1].x, trs[1].y, trs[1].z, trs[1].w)
		frame_data += struct.pack('3f', trs[2].x, trs[2].y, trs[2].z)

action_data = b''

#write sequence of current timeline:
def write_frame_range(name, first, last, relative='local'):
	global action_data
	action_data += write_string(name) #name (begin, end)
	action_data += struct.pack('I', frame_count) #first frame

	if relative == 'local':
		to_world = mathutils.Matrix()
	elif relative == 'first':
		inv_matrix_first = armature.matrix_world.copy()
		inv_matrix_first.invert()
	elif relative == 'global':
		bpy.context.scene.frame_set(first, subframe=0.0) #note: second param is sub-frame
		to_world = armature.matrix_world
	else:
		print("Don't understand root motion specifier '" + relative + "'; expecting local, first, or global")
		exit(1)

	for frame in range(first, last+1):
		bpy.context.scene.frame_set(frame, subframe=0.0) #note: second param is sub-frame
		if relative == 'first':
			to_world = inv_matrix_first * armature.matrix_world
		write_frame(armature.pose, root_xf=to_world)
	action_data += struct.pack('I', frame_count) #last frame
	print("Wrote '" + name + "' frames [" + str(first) + ", " + str(last) + "] mode '" + relative + "'")

#write action as name + series of frames:
def write_action(action, relative='local'):
	armature.animation_data.action = action
	write_frame_range(action.name, round(action.frame_range[0]), round(action.frame_range[1]), relative=relative)

#Write hierarchy, names, bind matrices for bones:
for bone in armature.data.bones:
	write_bone(bone)

#Frames will be stored a blocks of transforms, in bone index order:
idx_to_bone_name = [-1] * len(bone_name_to_idx)
for kv in bone_name_to_idx.items():
	idx = struct.unpack('i', kv[1])[0]
	assert(idx_to_bone_name[idx] == -1)
	idx_to_bone_name[idx] = kv[0]

#Now, for each action, write animation frames:
real_action_names = []
#(first pass: named frame ranges)
for action_name in action_names:
	m = re.match(r"^(.*)!(.*)$", action_name)
	relative = 'local'
	if m:
		action_name = m.group(1)
		relative = m.group(2)
	m = re.match(r"^\[(\d+),(\d+)\](.*)$", action_name)
	if m:
		first = int(m.group(1))
		last = int(m.group(2))
		name = m.group(3)
		print("Writing '" + action_name + "' as a frame range.")
		write_frame_range(name, first, last, relative=relative)
	else:
		real_action_names.append((action_name, relative))

for action_name in real_action_names:
	print("Writing '" + action_name[0] + "' as a real action.")
	write_action(bpy.data.actions[action_name[0]], action_name[1])

#----------------------------
#Extract animated mesh (==> vertex groups and weights)

vertex_data = b''

#Write mesh vertices to vertex_data, return packed (begin,end) range
def write_mesh(mesh, xf=mathutils.Matrix(), do_normal=True, do_color=True, do_uv=True, do_weights=True):
	global vertex_data

	itxf = xf.copy()
	itxf.transpose()
	itxf.invert()

	colors = None
	if do_color:
		if len(mesh.vertex_colors) == 0:
			print("WARNING: trying to export color data, but mesh '" + mesh.name + "' does not have color data; will output 0xffffffff")
		else:
			colors = mesh.vertex_colors.active.data

	uvs = None
	if do_uv:
		if len(mesh.uv_layers) == 0:
			print("WARNING: trying to export texcoord data, but mesh '" + mesh.name + "' does not uv data; will output (0.0, 0.0)")
		else:
			uvs = mesh.uv_layers.active.data


	#write the mesh:
	for poly in mesh.polygons:
		assert(len(poly.loop_indices) == 3)
		for i in range(0,3):
			assert(mesh.loops[poly.loop_indices[i]].vertex_index == poly.vertices[i])
			loop = mesh.loops[poly.loop_indices[i]]
			vertex = mesh.vertices[loop.vertex_index]

			position = xf @ vertex.co
			vertex_data += struct.pack('fff', *position)

			if do_normal:
				normal = itxf @ loop.normal
				normal.normalize()
				vertex_data += struct.pack('fff', *normal)

			if do_color:
				if colors != None:
					col = colors[poly.loop_indices[i]].color
					vertex_data += struct.pack('BBBB', int(col[0] * 255), int(col[1] * 255), int(col[2] * 255), int(col[3] * 255))
				else:
					vertex_data += struct.pack('BBBB', 255, 255, 255, 255)

			if do_uv:
				if uvs != None:
					uv = uvs[poly.loop_indices[i]].uv
					vertex_data += struct.pack('ff', uv.x, uv.y)
				else:
					vertex_data += struct.pack('ff', 0, 0)

			if do_weights:
				bone_weights = []
				for g in vertex.groups:
					group_name = obj.vertex_groups[g.group].name
					assert(group_name in bone_name_to_idx)
					bone_weights.append([g.weight, group_name])
				bone_weights.sort()
				bone_weights.reverse()
				if len(bone_weights) > 4:
					print("WARNING: clamping vertex with weights:")
					for bw in bone_weights:
						print("  " + str(bw[0]) + " for '" + bw[1] + "'")
					#trim:
					bone_weights = bone_weights[0:4]
				#normalize remaining weights:
				total = 0.0
				for bw in bone_weights:
					total += bw[0]
				for bw in bone_weights:
					bw[0] /= total
				if len(bone_weights) == 0:
					print("WARNING: vertex with no bone weights.")

				while len(bone_weights) < 4:
					bone_weights.append([0, idx_to_bone_name[0]])

				for bw in bone_weights:
					vertex_data += struct.pack('f', bw[0])

				for bw in bone_weights:
					idx = struct.unpack('I', bone_name_to_idx[bw[1]])[0]
					vertex_data += struct.pack('I', idx)

for obj in objs:
	#Get a copy of the mesh in the rest pose:
	armature.data.pose_position = 'REST'
	bpy.context.view_layer.update()
	#TODO: how to get 'RENDER' depsgraph?
	mesh = obj.to_mesh(preserve_all_data_layers=True, depsgraph=bpy.context.evaluated_depsgraph_get())

	#Triangulate the mesh:
	# from: https://blender.stackexchange.com/questions/45698/triangulate-mesh-in-python
	bm = bmesh.new()
	bm.from_mesh(mesh)
	bmesh.ops.triangulate(bm, faces=bm.faces[:], quad_method='BEAUTY', ngon_method='BEAUTY')
	bm.to_mesh(mesh)
	bm.free()

	#compute normals (respecting face smoothing):
	mesh.calc_normals_split()

	obj_to_arm = armature.matrix_world.copy()
	obj_to_arm.invert()
	obj_to_arm = obj_to_arm @ obj.matrix_world

	write_mesh(mesh, xf=obj_to_arm)

#----------------------------
#Write final animation file

#write the strings chunk and scene chunk to an output blob:
blob = open(outfile, 'wb')
def write_chunk(magic, data):
	blob.write(struct.pack('4s',magic)) #type
	blob.write(struct.pack('I', len(data))) #length
	blob.write(data)

write_chunk(b'str0', strings_data)
write_chunk(b'bon0', bone_data)
write_chunk(b'frm0', frame_data)
write_chunk(b'act0', action_data)
write_chunk(b'msh0', vertex_data)

print("Wrote " + str(blob.tell()) + " bytes [== "
	+ str(len(strings_data)) + " bytes of strings + "
	+ str(len(bone_data)) + " bytes of bone info + "
	+ str(len(frame_data)) + " bytes of frames + "
	+ str(len(action_data)) + " bytes of action info + "
	+ str(len(vertex_data)) + " bytes of mesh]"
	+ " to '" + outfile + "'")
blob.close()