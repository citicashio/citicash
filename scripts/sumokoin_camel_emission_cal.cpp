// Copyright (c) 2018, The CitiCash Project
// Sumokoin Block Reward Emission
// Short URL: http://cpp.sh/6eeyr

#include <iostream>
#include <iomanip>
#include <string>
#include <math.h>
#include <stdint.h>
#include "../src/cryptonote_config.h"

int main()
{
  uint64_t coins_already_generated = 0;
  uint64_t height = 0;
  uint64_t total_time = 0;
  uint64_t block_reward = 0;
  uint64_t cal_block_reward = 0;
  double money_supply_percentage;
  uint64_t round_factor = 10000000;
  uint64_t count = 0;
  bool print_by_year = false;

  std::cout << "Height" << "\t" << "Reward" << "\t" << "Coins Gen" << "\t" << "(%)" << "\t" << "Day" << "\t" << "Year" << "\n";

  while (coins_already_generated < (MONEY_SUPPLY - FINAL_SUBSIDY)){
    bool emission_speed_change_happened = false;
    if (height % COIN_EMISSION_HEIGHT_INTERVAL == 0){
      if (height < (PEAK_COIN_EMISSION_HEIGHT + COIN_EMISSION_HEIGHT_INTERVAL)){
        uint64_t interval_num = height / COIN_EMISSION_HEIGHT_INTERVAL;
        money_supply_percentage = 0.1888 + interval_num*(0.023 + interval_num*0.0032);
        cal_block_reward = ((uint64_t)(MONEY_SUPPLY * money_supply_percentage)) >> EMISSION_SPEED_FACTOR;
      }
      else{
        cal_block_reward = (MONEY_SUPPLY - coins_already_generated) >> EMISSION_SPEED_FACTOR;
      }
      emission_speed_change_happened = true;
      count++;
    }

    if (height == 0){
      block_reward = GENESIS_BLOCK_REWARD;
    }
    else{
      block_reward = cal_block_reward / round_factor * round_factor;
    }

    if (block_reward < FINAL_SUBSIDY){
      if (MONEY_SUPPLY > coins_already_generated){
        block_reward = FINAL_SUBSIDY;
      }
      else{
        block_reward = FINAL_SUBSIDY / 2;
      }
    }

    coins_already_generated += block_reward;
    total_time += DIFFICULTY_TARGET;

    if (emission_speed_change_happened && (print_by_year ? count % 2 : true)){
      if (height == 0){
        std::cout << height << "\t"
          << std::fixed << std::setprecision(2) << block_reward / 1000000000.0 << "\t"
          << std::setprecision(8) << coins_already_generated / 1000000000.0 << "\t"
          << std::fixed << std::setprecision(2) << coins_already_generated*100.0 / MONEY_SUPPLY << "\t"
          << std::fixed << std::setprecision(0) << total_time / (60 * 60 * 24.0) << "\t"
          << std::setprecision(2) << total_time / (60 * 60 * 24.0) / 365.25 << "\n";
      }
      else{
        std::cout << height << "\t"
          << std::fixed << std::setprecision(2) << block_reward / 1000000000.0 << "\t"
          << std::setprecision(2) << coins_already_generated / 1000000000.0 << "\t"
          << std::fixed << std::setprecision(2) << coins_already_generated*100.0 / MONEY_SUPPLY << "\t"
          << std::fixed << std::setprecision(0) << total_time / (60 * 60 * 24.0) << "\t"
          << std::setprecision(2) << total_time / (60 * 60 * 24.0) / 365.25 << "\n";
      }
    }

    height += 1;
  }

  std::cout << height << "\t"
    << std::fixed << std::setprecision(2) << block_reward / 1000000000.0 << "\t"
    << std::setprecision(2) << coins_already_generated / 1000000000.0 << "\t"
    << std::fixed << std::setprecision(2) << coins_already_generated*100.0 / MONEY_SUPPLY << "\t"
    << std::fixed << std::setprecision(0) << total_time / (60 * 60 * 24.0) << "\t"
    << std::setprecision(2) << total_time / (60 * 60 * 24.0) / 365.25 << "\n";

}
