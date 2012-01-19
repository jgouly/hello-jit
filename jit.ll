; ModuleID = 'jit.ll'

@stack = external global [999 x i32]
@sp = external global i32*

declare void @push1()
