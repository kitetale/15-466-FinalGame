.PHONY : all

#n.b. the '-y' sets autoexec scripts to 'on' so that driver expressions will work
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
	BLENDER = /Applications/blender.app/Contents/MacOS/blender -y
else ifeq ($(UNAME_S),Linux)
	BLENDER = ../../blender-2.93.4-linux-x64/blender -y
else
	BLENDER = blender
endif

EXPORT_MESHES=export-meshes.py
EXPORT_WALKMESHES=export-walkmeshes.py
EXPORT_SCENE=export-scene.py
EXPORT_ANIMATION=export-bone-animations.py

DIST=../dist

all : \
	$(DIST)/worm.pnct \
	$(DIST)/worm.banims \
	$(DIST)/worm.w \
	$(DIST)/worm.scene 


$(DIST)/worm.pnct : worm.blend $(EXPORT_MESHES)
	$(BLENDER) --background --python $(EXPORT_MESHES) -- '$<':Platforms '$@'

$(DIST)/worm.banims : worm.blend $(EXPORT_ANIMATION)
	$(BLENDER) --background --python $(EXPORT_ANIMATION) -- '$<' 'Armature' '[0,30]Crawl' '$@'

$(DIST)/worm.w : worm.blend $(EXPORT_WALKMESHES)
	$(BLENDER) --background --python $(EXPORT_WALKMESHES) -- '$<':WalkMeshes '$@'

$(DIST)/worm.scene : worm.blend $(EXPORT_SCENE)
	$(BLENDER) --background --python $(EXPORT_SCENE) -- '$<':Platforms '$@'

# $(DIST)/proto.pnct : proto.blend $(EXPORT_MESHES)
# 	$(BLENDER) --background --python $(EXPORT_MESHES) -- '$<':Platforms '$@'

# $(DIST)/proto.scene : proto.blend $(EXPORT_SCENE)
# 	$(BLENDER) --background --python $(EXPORT_SCENE) -- '$<':Platforms '$@'

# $(DIST)/proto.w : proto.blend $(EXPORT_WALKMESHES)
# 	$(BLENDER) --background --python $(EXPORT_WALKMESHES) -- '$<':WalkMeshes '$@'