// Copyright (c) 2011-2017 The Cryptonote developers
// Copyright (c) 2017-2018 The Circle Foundation +ACY- Conceal Devs
// Copyright (c) 2018-2019 Conceal Network +ACY- Conceal Devs
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

+ACM-include +ADw-string.h+AD4-
+ACM-include +ACI-context.h+ACI-

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
