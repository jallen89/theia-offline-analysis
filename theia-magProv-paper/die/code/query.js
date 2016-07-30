avl.query({
  "codedMark": Enc({"src":ID(A),"dst":ID(B)},S(AB))
});
Return:
  Enc{ip, port, time}  //encrypted w/ S(AB) 

avl.insert({
  "login": {"id": ID(A), "text": nounce, "sig": Sig(nounce, PVK(A))},
  "codedMark": Enc({"src":ID(A),"dst":ID(B)},S(AB)),
  "networkLoc": 
       Enc{new_ip, new_port, new_time}  //encrypted w/ S(AB)
});

avl.getRandomPeers(numRequest);
Return:
  ??
