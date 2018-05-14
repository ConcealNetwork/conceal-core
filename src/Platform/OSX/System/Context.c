// Copyright (c) 2011-2015 The Cryptonote developers
// Copyright (c) 2015-2016 The Bytecoin developers
// Copyright (c) 2016-2017 The TurtleCoin developers
// Copyright (c) 2017-2018 krypt0x aka krypt0chaos
// Copyright (c) 2018 The Circle Foundation
//
// This file is part of Conceal Sense Crypto Engine.
//
// Conceal is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// Conceal is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY+ADs- without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with Conceal.  If not, see +ADw-http://www.gnu.org/licenses/+AD4-.

+ACM-include +ADw-string.h+AD4-
+ACM-include +ACI-Context.h+ACI-

void
makecontext(uctx +ACo-ucp, void (+ACo-func)(void), intptr+AF8-t arg)
+AHs-
  long +ACo-sp+ADs-
  
  memset(+ACY-ucp-+AD4-uc+AF8-mcontext, 0, sizeof ucp-+AD4-uc+AF8-mcontext)+ADs-
  ucp-+AD4-uc+AF8-mcontext.mc+AF8-rdi +AD0- (long)arg+ADs-
  sp +AD0- (long+ACo-)ucp-+AD4-uc+AF8-stack.ss+AF8-sp+ucoAPg-uc+AF8-stack.ss+AF8-size/sizeof(long)+ADs-
  sp -+AD0- 1+ADs-
  sp +AD0- (void+ACo-)((uintptr+AF8-t)sp - (uintptr+AF8-t)sp+ACU-16)+ADs-	/+ACo- 16-align for OS X +ACo-/
  +ACo---sp +AD0- 0+ADs-	/+ACo- return address +ACo-/
  ucp-+AD4-uc+AF8-mcontext.mc+AF8-rip +AD0- (long)func+ADs-
  ucp-+AD4-uc+AF8-mcontext.mc+AF8-rsp +AD0- (long)sp+ADs-
+AH0-

int
swapcontext(uctx +ACo-oucp, const uctx +ACo-ucp)
+AHs-
  if(getcontext(oucp) +AD0APQ- 0)
    setcontext(ucp)+ADs-
  return 0+ADs-
+AH0-
