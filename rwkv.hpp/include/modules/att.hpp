#include "hvml/tensor.hpp"

#include "safetensors/safetensors.hpp"
#include "modules/timeshift.hpp"
#include "modules/linear.hpp"
class RWKV_5_ATT
{
    public:
        uint head_size = 64;
        uint n_head; 
        TimeShift timeshift;
        Tensor<float> time_mix_k;
        Tensor<float> time_mix_v;
        Tensor<float> time_mix_r;
        Tensor<float> time_mix_g;
        Tensor<float> time_decay;
        Tensor<float> time_faaaa;
        Tensor<float> state;
        Tensor<float> buffer;
        Linear receptance;
        Linear key;
        Linear value;
        Linear gate;
        Linear output;
        GroupNorm ln_x;

        RWKV_5_ATT(){
        }
        
        RWKV_5_ATT(int layerID, safetensors::safetensors_t& model, ulong max_batch, ulong max_seq){
            // std::cout << "RWKV_5_ATTcreate:" << layerID << std::endl;
            std::string prefix = "blocks." + std::to_string(layerID) + ".att.";

            this->time_mix_k = model[prefix + "time_mix_k"][0][0];
            this->time_mix_v = model[prefix + "time_mix_v"][0][0];
            this->time_mix_r = model[prefix + "time_mix_r"][0][0];
            this->time_mix_g = model[prefix + "time_mix_g"][0][0];

            auto dims = this->time_mix_k.shape[0];

            // std::cout << "time_mix_k:" << dims << std::endl;

            this->n_head = dims/this->head_size;
            this->state = Tensor<float>({max_batch*max_seq, this->n_head , this->head_size, this->head_size},0.0f);
            // std::cout << "n_head:" << this->n_head << std::endl;
            
            this->time_decay = model[prefix + "time_decay"];
            this->time_faaaa = model[prefix + "time_faaaa"];
            this->buffer = Tensor<float>({max_batch, max_seq, dims});

            this->timeshift = TimeShift(max_batch, max_seq, dims);

            this->receptance = Linear(model, prefix + "receptance", max_batch, max_seq);
            this->key = Linear(model, prefix + "key", max_batch, max_seq);
            this->value = Linear(model, prefix + "value", max_batch, max_seq);
            this->gate = Linear(model, prefix + "gate", max_batch, max_seq);
            this->output = Linear(model, prefix + "output", max_batch, max_seq);
            this->ln_x = GroupNorm(model[prefix + "ln_x.weight"], model[prefix + "ln_x.bias"], n_head, max_batch, max_seq);
            
        }
        Tensor<float> operator()(Tensor<float>& input){
            this->buffer.unsafereshape({input.shape[0], input.shape[1], input.shape[2]});
         
            auto xx = this->timeshift(input);
            this->time_mix_k.lerp(xx, input, this->buffer);
            auto k = this->key(this->buffer);
            this->time_mix_v.lerp(xx, input, this->buffer);
            auto v = this->value(this->buffer);
            this->time_mix_r.lerp(xx, input, this->buffer);
            auto r = this->receptance(this->buffer);
            this->time_mix_g.lerp(xx, input, this->buffer);
            auto gv = this->gate(this->buffer);
            
           
    
            this->state.wkv5(r,k,v,this->time_decay,this->time_faaaa, this->buffer);
            
            // std::cout << "WKVOUT:" << crbuff[0][0] << std::endl;
            
         
            this->buffer.multiply(1./8, this->buffer);

            // std::cout << "WKVOUT/8:" << crbuff[0][0] << std::endl;
            

            // std::cout << crbuff[0][0] << std::endl;
          
            auto xxa = this->ln_x(this->buffer);
            /*
            tensor([-0.0285, -0.0082, -0.0168,  0.0204,  0.0476])
            */
            // std::cout << "xxa:" << xxa[0][0] << std::endl;
            gv.sigmoid(this->buffer);
            this->buffer.multiply(gv, this->buffer);
            
            xxa.multiply(this->buffer, this->buffer);
            
         
            auto xout = this->output(this->buffer);

            
            return xout;


        }

};