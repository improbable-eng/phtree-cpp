cmake_minimum_required(VERSION 3.14)

target_sources(phtree
        PRIVATE
        debug_helper.h
        node.h
        ph_entry.h
        ph_iterator_base.h
        ph_iterator_full.h
        ph_iterator_hc.h
        ph_iterator_knn_hs.h
        ph_iterator_simple.h
        phtree_v16.h
        )
