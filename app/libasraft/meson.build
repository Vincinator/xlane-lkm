#project('libasraft', 'c', default_options : ['default_library=static'])
#cmake_c_standard = '99'
#threads_prefer_pthread_flag = 'ON'
# include_directories('deps/inih')
# include_directories('.')
#PkgConfig_dep = dependency('PkgConfig')
#Threads_dep = dependency('Threads')
#libuuid_library = find_library('LibUUID_LIBRARY')
#cmake_build_type = 'Debug'
# include_directories(libuuid_include_dir)
# configure_file(test/node.ini node.ini COPYONLY)

name = 'libasraft'
warning_level = 0

sources = files('runner.c',
            'libasraft.c',
            'logger.c',
            'consensus.c',
            'kvstore.c',
            'pacemaker.c',
            'payload.c',
            'ringbuffer.c',
            'membership.c',
            'leader.c',
            'follower.c',
            'candidate.c',
            'replication.c',
            'pktqueue.c',
            'tnode.c',
            'pkthandler.c',
            'proto.c',
            'multicast.c',
            'eval_helper.c',
            'ini.c')
# target_link_libraries(['libasraft', 'PRIVATE', 'Threads::Threads'])
# target_link_libraries(['testrunner', 'LINK_PUBLIC', 'inih'])
# target_link_libraries(['testrunner', 'LINK_PUBLIC', 'libasraft'])
