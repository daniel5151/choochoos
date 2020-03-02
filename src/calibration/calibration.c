// generated code, do not edit
// generated by 'python calibration/process.py'
// clang-format off

#include "calibration.h"

int calibration_index_of_train(int train) {
  switch (train) {
    case 1: return 0;
    case 24: return 1;
    case 58: return 2;
    case 74: return 3;
    case 78: return 4;
    case 79: return 5;
    default: return -1;
  }
}

void fill_calibration_data(struct calibration_data_t* c) {
  memset(c, 0, sizeof(struct calibration_data_t));
  c->trains[0].train = 1;
  c->trains[0].speeds[14].expected_velocity_mmps = 650;
  c->trains[0].speeds[14].measured = true;
  c->trains[0].speeds[13].expected_velocity_mmps = 577;
  c->trains[0].speeds[13].measured = true;
  c->trains[0].speeds[12].expected_velocity_mmps = 494;
  c->trains[0].speeds[12].measured = true;
  c->trains[0].speeds[11].expected_velocity_mmps = 418;
  c->trains[0].speeds[11].measured = true;
  c->trains[0].speeds[10].expected_velocity_mmps = 364;
  c->trains[0].speeds[10].measured = true;
  c->trains[0].speeds[9].expected_velocity_mmps = 300;
  c->trains[0].speeds[9].measured = true;
  c->trains[0].speeds[8].expected_velocity_mmps = 237;
  c->trains[0].speeds[8].measured = true;
  c->trains[0].speeds[7].expected_velocity_mmps = 177;
  c->trains[0].speeds[7].measured = true;
  c->trains[0].speeds[6].expected_velocity_mmps = 121;
  c->trains[0].speeds[6].measured = true;
  c->trains[0].speeds[5].expected_velocity_mmps = 77;
  c->trains[0].speeds[5].measured = true;
  c->trains[0].speeds[4].expected_velocity_mmps = 43;
  c->trains[0].speeds[4].measured = true;
  c->trains[1].train = 24;
  c->trains[1].speeds[14].expected_velocity_mmps = 576;
  c->trains[1].speeds[14].measured = true;
  c->trains[1].speeds[13].expected_velocity_mmps = 590;
  c->trains[1].speeds[13].measured = true;
  c->trains[1].speeds[12].expected_velocity_mmps = 507;
  c->trains[1].speeds[12].measured = true;
  c->trains[1].speeds[11].expected_velocity_mmps = 443;
  c->trains[1].speeds[11].measured = true;
  c->trains[1].speeds[10].expected_velocity_mmps = 363;
  c->trains[1].speeds[10].measured = true;
  c->trains[1].speeds[9].expected_velocity_mmps = 304;
  c->trains[1].speeds[9].measured = true;
  c->trains[1].speeds[8].expected_velocity_mmps = 230;
  c->trains[1].speeds[8].measured = true;
  c->trains[1].speeds[7].expected_velocity_mmps = 172;
  c->trains[1].speeds[7].measured = true;
  c->trains[1].speeds[6].expected_velocity_mmps = 125;
  c->trains[1].speeds[6].measured = true;
  c->trains[1].speeds[5].expected_velocity_mmps = 81;
  c->trains[1].speeds[5].measured = true;
  c->trains[1].speeds[4].expected_velocity_mmps = 42;
  c->trains[1].speeds[4].measured = true;
  c->trains[2].train = 58;
  c->trains[2].speeds[14].expected_velocity_mmps = 625;
  c->trains[2].speeds[14].measured = true;
  c->trains[2].speeds[13].expected_velocity_mmps = 549;
  c->trains[2].speeds[13].measured = true;
  c->trains[2].speeds[12].expected_velocity_mmps = 479;
  c->trains[2].speeds[12].measured = true;
  c->trains[2].speeds[11].expected_velocity_mmps = 413;
  c->trains[2].speeds[11].measured = true;
  c->trains[2].speeds[10].expected_velocity_mmps = 336;
  c->trains[2].speeds[10].measured = true;
  c->trains[2].speeds[9].expected_velocity_mmps = 274;
  c->trains[2].speeds[9].measured = true;
  c->trains[2].speeds[8].expected_velocity_mmps = 206;
  c->trains[2].speeds[8].measured = true;
  c->trains[2].speeds[7].expected_velocity_mmps = 151;
  c->trains[2].speeds[7].measured = true;
  c->trains[2].speeds[6].expected_velocity_mmps = 113;
  c->trains[2].speeds[6].measured = true;
  c->trains[2].speeds[5].expected_velocity_mmps = 72;
  c->trains[2].speeds[5].measured = true;
  c->trains[2].speeds[4].expected_velocity_mmps = 33;
  c->trains[2].speeds[4].measured = true;
  c->trains[2].speeds[3].expected_velocity_mmps = 18;
  c->trains[2].speeds[3].measured = true;
  c->trains[2].speeds[2].expected_velocity_mmps = 16;
  c->trains[2].speeds[2].measured = true;
  c->trains[2].speeds[1].expected_velocity_mmps = 13;
  c->trains[2].speeds[1].measured = true;
  c->trains[3].train = 74;
  c->trains[3].speeds[14].expected_velocity_mmps = 650;
  c->trains[3].speeds[14].measured = true;
  c->trains[3].speeds[13].expected_velocity_mmps = 663;
  c->trains[3].speeds[13].measured = true;
  c->trains[3].speeds[12].expected_velocity_mmps = 646;
  c->trains[3].speeds[12].measured = true;
  c->trains[3].speeds[11].expected_velocity_mmps = 592;
  c->trains[3].speeds[11].measured = true;
  c->trains[3].speeds[10].expected_velocity_mmps = 545;
  c->trains[3].speeds[10].measured = true;
  c->trains[3].speeds[9].expected_velocity_mmps = 472;
  c->trains[3].speeds[9].measured = true;
  c->trains[3].speeds[8].expected_velocity_mmps = 410;
  c->trains[3].speeds[8].measured = true;
  c->trains[3].speeds[7].expected_velocity_mmps = 357;
  c->trains[3].speeds[7].measured = true;
  c->trains[3].speeds[6].expected_velocity_mmps = 298;
  c->trains[3].speeds[6].measured = true;
  c->trains[3].speeds[5].expected_velocity_mmps = 250;
  c->trains[3].speeds[5].measured = true;
  c->trains[3].speeds[4].expected_velocity_mmps = 193;
  c->trains[3].speeds[4].measured = true;
  c->trains[3].speeds[3].expected_velocity_mmps = 146;
  c->trains[3].speeds[3].measured = true;
  c->trains[3].speeds[2].expected_velocity_mmps = 88;
  c->trains[3].speeds[2].measured = true;
  c->trains[4].train = 78;
  c->trains[4].speeds[14].expected_velocity_mmps = 506;
  c->trains[4].speeds[14].measured = true;
  c->trains[4].speeds[13].expected_velocity_mmps = 461;
  c->trains[4].speeds[13].measured = true;
  c->trains[4].speeds[12].expected_velocity_mmps = 394;
  c->trains[4].speeds[12].measured = true;
  c->trains[4].speeds[11].expected_velocity_mmps = 348;
  c->trains[4].speeds[11].measured = true;
  c->trains[4].speeds[10].expected_velocity_mmps = 288;
  c->trains[4].speeds[10].measured = true;
  c->trains[4].speeds[9].expected_velocity_mmps = 249;
  c->trains[4].speeds[9].measured = true;
  c->trains[4].speeds[8].expected_velocity_mmps = 187;
  c->trains[4].speeds[8].measured = true;
  c->trains[4].speeds[7].expected_velocity_mmps = 138;
  c->trains[4].speeds[7].measured = true;
  c->trains[4].speeds[6].expected_velocity_mmps = 94;
  c->trains[4].speeds[6].measured = true;
  c->trains[5].train = 79;
  c->trains[5].speeds[14].expected_velocity_mmps = 726;
  c->trains[5].speeds[14].measured = true;
  c->trains[5].speeds[13].expected_velocity_mmps = 631;
  c->trains[5].speeds[13].measured = true;
  c->trains[5].speeds[12].expected_velocity_mmps = 545;
  c->trains[5].speeds[12].measured = true;
  c->trains[5].speeds[11].expected_velocity_mmps = 470;
  c->trains[5].speeds[11].measured = true;
  c->trains[5].speeds[10].expected_velocity_mmps = 399;
  c->trains[5].speeds[10].measured = true;
  c->trains[5].speeds[9].expected_velocity_mmps = 339;
  c->trains[5].speeds[9].measured = true;
  c->trains[5].speeds[8].expected_velocity_mmps = 283;
  c->trains[5].speeds[8].measured = true;
  c->trains[5].speeds[7].expected_velocity_mmps = 226;
  c->trains[5].speeds[7].measured = true;
  c->trains[5].speeds[6].expected_velocity_mmps = 154;
  c->trains[5].speeds[6].measured = true;
  c->trains[5].speeds[5].expected_velocity_mmps = 118;
  c->trains[5].speeds[5].measured = true;
  c->trains[5].speeds[4].expected_velocity_mmps = 76;
  c->trains[5].speeds[4].measured = true;
  c->trains[5].speeds[3].expected_velocity_mmps = 54;
  c->trains[5].speeds[3].measured = true;
  c->trains[5].speeds[2].expected_velocity_mmps = 31;
  c->trains[5].speeds[2].measured = true;
}
