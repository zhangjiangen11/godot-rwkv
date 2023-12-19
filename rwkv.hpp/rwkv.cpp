#include <iostream>
#include <string>
#include <fstream>
#include "rwkv.hpp"
#include "sampler/sample.hpp"
#include "tokenizer/tokenizer.hpp"
int main(){
    std::cout << "Hello World" << std::endl;
    std::string path = "./model.safetensors";

    RWKVTokenizer worldTokenizer("rwkv_vocab_v20230424.txt");
    
    auto tokens = worldTokenizer.encode("### Instruction: Create a harry potter fanfiction. \n\n### Response:");
    
    std::cout << worldTokenizer.decode(tokens) << std::endl;
    std::cout << "Loading model" << std::endl;

    // allocating ram for 50 tokens simultaneously
    // used for allocations of static memory usage

    RWKV model(path, 32, 2);

    

    std::cout << "Model loaded" << std::endl;
    

    auto logits = model({tokens});

    std::cout << logits << std::endl;

    model.set_state(model.new_state());


    // cudaSetDevice(0);
    // model.toVulkan();

    logits = model({tokens});

    std::cout << logits << std::endl;

    // model.set_state(model.new_state());

    

    // std::cout << "Model sent to vulkan" << std::endl;

    // logits = model({tokens});

    // std::cout << logits << std::endl;


    ulong tokenstogen = 100;
    std::vector<ulong> generated;
    for (int i = 0; i < tokenstogen; i++)
    {
        // std::cout << "Generating token " << i << std::endl;
        auto sample = ulong(typical(logits[0][logits.shape[1]-1].data, 0.9, 0.9));

        generated.push_back(sample);

        std::cout.flush();
        std::cout << worldTokenizer.decode({sample});

        logits = model({{sample}});
        // std::cout << logits << std::endl;
    }

    
    return 0;
}