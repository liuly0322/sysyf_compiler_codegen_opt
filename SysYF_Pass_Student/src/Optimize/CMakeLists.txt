add_library(
        SysYFPass STATIC
        Pass.cpp
        DominateTree.cpp
        RDominateTree.cpp
        Mem2Reg.cpp
        ActiveVar.cpp
        Check.cpp
        PureFunction.cpp
        SCCP.cpp
        DeadCode.cpp
        CSE.cpp
)
