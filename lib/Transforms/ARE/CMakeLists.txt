add_definitions(-DDRAGONBALL_DEBUG)

add_llvm_loadable_module(DragonBallARE
  ARE.cpp

  DEPENDS
  intrinsics_gen
  PLUGIN_TOOL
  opt
)
