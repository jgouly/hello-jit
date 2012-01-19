; ModuleID = 'jit.ll'

@stack = external global [999 x i32]
@sp = external global i32*

define void @push1() {
  %1 = load i32** @sp
  %2 = getelementptr inbounds i32* %1, i32 1
  store i32* %2, i32** @sp
  store i32 5, i32* %1
  ret void
}
