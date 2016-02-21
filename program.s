#very simple program to test a branch predictor
# the code roughly corresponds to the following C program
# int i = 0; int n = 2000
# int a = 0; int b = 0;
# while(i != n) {
#  if (i & 1) {
#    a += 1;
#  } else {
#    b += 1;
#  }
#  i+=1;
# }
#
#00
ADDIU $1, $0, 200

#04
ADDIU $3, $3, 1

#random work

#08
ANDI $4, $3, 1

#12
BNE $4, $0, 12
#16
ADDIU $5, $5, 1
#20
BEQ $0, $0, 8

#24
ADDIU $6, $6, 1

#go to top as long as i != 20
#28
BNE $1, $3, -24

#dump the register file
#SYS $30, $0, 0

#exit
SYS $31, $0, 0

