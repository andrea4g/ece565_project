#ifndef FU_H
#define FU_H

class FU
{
    public:
        int type;             // FU-type
        int id;               // FU-id
        vector<FU *> fanin;   // 
        vector<FU *> fanout;
        vector<int> busy;
        
        set<int> future_parents;  // Operations that will be connected
                                  // as parents due op. binded to this
                                  // functional unit.
        set<int> future_children; // Operations that will be connected
                                  // as children due op. binded to this
                                  // FU.

        vector<int> port1;    // store reg_id
        vector<int> port2;    // store reg_id
        int weight();  
        bool rabid();    
    public:
        CDog();
        ~CDog();
    // Attributes
        void setRabid(bool x);
        bool getRabid();
        void setName(string x);
        string getName();
        void setWeight(int x);
        int getWeight();

    // Behaviours 
        void growl();
        void eat();
};

#endif // FU_H
