add_subdirectory(demo)
aux_source_directory(. SRUN_GUI_SOURCES)
add_executable(srun_gui ${SRUN_GUI_SOURCES})
