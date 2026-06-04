Let's explain each and every memory ordering in deep
let's start with the exchange in enque
used: acq_rel; why? to maintain coherency between the producer threads for the loading 
of the memory space corresponding to (new_node->next) the data and the atomic pointer inside 
we set the values after creating the instance like nullptr and data and mark them relaxed 
P1 is creating the new node and setting the values it should with coherrent with the 
P2 when ('old_node->next').store .. loading of the old_node->next pointer's memory 
so P1 release and P2 acquire for the later steps of exchange so need acq_rel

Now about synchronising the values filled in the next of the enque node 
let's take example of the zero node P1 and C1 are in race just after the cold start 
P1 exchanged the tail with new but now updating the old_node's next variable
along with the C1 which find the old_head now checking for the old->next 
if P1 updated its values it must reflect in C1 so need release in p1 and acquire in C1 

C1 is the only thread operating on the head so there is no need to put any ordering relaxed will be fine 


in deque I used memory_order_relaxed its because I was not able to find the acquire partner corresponding to if I use release here 
head.store(new_head, std::memory_order_relaxed)