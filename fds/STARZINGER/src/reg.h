#ifndef REG_H
#define REG_H

class Register
{
    public:
        int id;               // FU-id
        vector<FU *> fanin;   // 
        vector<FU *> fanout;
        vector<int> busy;     // Represents_lifetime

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
