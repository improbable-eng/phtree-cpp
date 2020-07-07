cmake_minimum_required(VERSION 3.12)

target_sources(phtree
        PRIVATE
        ph_common.h
        ph_base_types.h
        ph_bits.h
        ph_distance.h
        ph_filter.h
        ph_flat_array_map.h
        ph_flat_sparse_map.h
        ph_preprocessor.h
        ph_tree_debug_helper.h
        ph_tree_stats.h
        )
