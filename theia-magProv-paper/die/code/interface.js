avl.getFrndReq({
  "codedID": Hash(ID(A))
});
Return:
  Enc{nounce1, ID(B)}  //encrypted w/ PK(A) 

avl.insFrndReq({
  "codedID": Hash(ID(A)),
  "friendReq": 
       Enc{nounce, ID(C)}  //encrypted w/ PK(A)
});
