#include "c_stdio.h"
#include "c_stdlib.h"
#include "c_string.h"
#include "user_interface.h"
#include "smart.h"
#include "pm/swtimer.h"

#define ADDR_MAP_NUM 10

static os_timer_t smart_timer;

static smart_addr_map *am[ADDR_MAP_NUM];

static smart_addr_map *matched = NULL;

static struct station_config *sta_conf;

static int cur_channel = 1;

static uint8_t mode = STATION_MODE;

static uint8_t alldone = 0;

// 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000(LSB)
// when the bit is set, means the ssid byte is got
static uint8_t *got_ssid = NULL;
static uint8_t *got_password = NULL;

static uint8_t *ssid_nibble = NULL;
static uint8_t *password_nibble = NULL;

static smart_succeed succeed = NULL;
static void *smart_succeed_arg = NULL;

void smart_end();
int smart_check(uint8_t *nibble, uint16_t len, uint8_t *dst, uint8_t *got){
  if(len == 0) 
    return 0;
  uint16_t dst_len = len/NIBBLE_PER_BYTE;
  uint16_t byte_num = 0, bit_num = 0;
  int i = 0, res = 1; // assume ok.
  c_memset(dst,0,dst_len);
  
  if(NIBBLE_PER_BYTE==1){
    for(i=0;i<len;i++){
      byte_num = (i) / 8;
      bit_num = (i) % 8;
      if(0x20>nibble[i] || nibble[i]>=0x7F){ // not printable
        NODE_DBG("Smart: got np byte %d:%02x\n", i, nibble[i]);
        nibble[i] = 0;
        got[byte_num] &= ~(0x1 << bit_num);  // clear the bit
        res = 0;  // not ok
      } else {
        dst[i] = nibble[i];
      }
    }
    return res;
  }

  // NIBBLE_PER_BYTE == 2
  if((len%NIBBLE_PER_BYTE) != 0){
    // this should not happen
    NODE_DBG("Smart: smart_check got odd len\n");
    return 0;
  }

  if(len == 2){
    // only one byte
    if(nibble[0]<=0xF && ((nibble[0]^0x1)&0xF == (nibble[1]>>4)) ){
      dst[0] = ((nibble[0]&0xF)<<4) + (nibble[1]&0xF);
      res = 1;
    }else{
      nibble[0] = 0;
      nibble[1] = 0;
      got[0] &= ~(0x3 << 0);  // clear the 0 bit
      res = 0;  // not ok
    }
    return res;
  }

  res = 1;  // assume ok.
  for(i=len-2;i>0;i--){
    bool forward = ( ((nibble[i]&0xF)^((i+1)&0xF)) == (nibble[i+1]>>4) );
    bool back = ( ((nibble[i-1]&0xF)^(i&0xF)) == (nibble[i]>>4) );
    if(!forward || !back){ 
    // wrong forward, or wrong back, replace i-1, i and i+1, until get right back, forward
      NODE_DBG("check: wf %d:%02x %02x %02x\n",i,nibble[i-1],nibble[i], nibble[i+1]);
      byte_num = (i-1) / 8;
      bit_num = (i-1) % 8;
      nibble[i-1] = 0;
      got[byte_num] &= ~(0x1 << bit_num);  // clear the bit

      byte_num = (i) / 8;
      bit_num = (i) % 8;
      nibble[i] = 0;
      got[byte_num] &= ~(0x1 << bit_num);  // clear the bit

      byte_num = (i+1) / 8;
      bit_num = (i+1) % 8;
      nibble[i+1] = 0;
      got[byte_num] &= ~(0x1 << bit_num);  // clear the bit
      res = 0;
      return res; // once there is error, 
    }

    if((i%NIBBLE_PER_BYTE) == 0) { // i == even
      dst[i/NIBBLE_PER_BYTE] = ((nibble[i]&0xF)<<4) + (nibble[i+1]&0xF);
    }
  }

  if(i==0){
    dst[0] = ((nibble[0]&0xF)<<4) + (nibble[1]&0xF);
  }

  for(i=0;i<dst_len;i++){   // check for non-printable byte
    // NODE_DBG("nibble %d:%02x %02x->%02x\n", i, nibble[i*NIBBLE_PER_BYTE], nibble[i*NIBBLE_PER_BYTE+1], dst[i]);
    byte_num = (i*NIBBLE_PER_BYTE) / 8;
    bit_num = (i*NIBBLE_PER_BYTE) % 8;
    if(0x20>dst[i] || dst[i]>=0x7F){ // not printable
      NODE_DBG("Smart: got np byte %d:%02x\n", i, dst[i]);
      dst[i] = 0; // reset byte
      nibble[i*NIBBLE_PER_BYTE] = 0;  // reset hi-nibble
      nibble[i*NIBBLE_PER_BYTE+1] = 0;  // reset lo-nibble
      got[byte_num] &= ~(0x3 << bit_num);  // clear the bit
      res = 0;  // not ok
    } 
  }
  return res;
}

void detect(uint8 *arg, uint16 len){
  uint16_t seq;
  int16_t seq_delta = 0;
  uint16_t byte_num = 0, bit_num = 0;
  int16_t c = 0;
  uint8 *buf = NULL;
  if( len == 12 ){
    return;
  } else if (len >= 64){
    buf = arg + sizeof(struct RxControl);
  } else {
    return;
  }
  if( ( (buf[0]) & TYPE_SUBTYPE_MASK) != TYPE_SUBTYPE_QOS_DATA){
    return;
  }
  if( (buf[1] & DS_RETRY_MASK) != NO_RETRY )
    return;
  if( buf[SEQ_ADDR] & 0xF != 0 )    // Fragment Number should = 0
    return;
  // calculate current seq number
  seq = buf[SEQ_ADDR+1];
  seq = seq<<4;
  seq += buf[SEQ_ADDR]>>4;

  if(!matched){     // cur_base_seq is ref to flag[0] when finding the patern
    int i;
    for (i = 0; i < ADDR_MAP_NUM; i++)  // for each source-dest adress pair in the map
    {
      if ( am[i]->flag_match_num == 0 ){  // not in the map yet
        if ( len - am[i]->base_len == am[i]->flag[0]) // store new source-dest adress pair to the map until flag[0] is got
        {
          // BSSID, SA, DA, store the SA, DA
          c_memcpy(am[i]->addr, &buf[ADDR_MATCH_START], ADDR_MATCH_LENGTH);  
          am[i]->flag_match_num++;    // =1
          am[i]->cur_base_seq = seq;  // assume the first seq is found
          am[i]->base_seq_valid = 1;
          // NODE_DBG("Smart: new addr pair found\n");
        }
        break;    // break any way for the next packet to come
      } 
      else if(0 == c_memcmp(am[i]->addr, &buf[ADDR_MATCH_START], ADDR_MATCH_LENGTH)){   // source-dest adress pair match
        if(am[i]->base_seq_valid == 0){
          if ( len - am[i]->base_len == am[i]->flag[0]) { // found the new flag[0]
            // here flag_match_num is already = 1
            am[i]->cur_base_seq = seq;
            am[i]->base_seq_valid = 1;  // the seq number is valid now
            // NODE_DBG("Smart: new base_seq found\n");
          }
          break;  // break any way for the next packet to come
        }

        // base seq number is valid, cal the delta
        if(seq >= am[i]->cur_base_seq){    
          seq_delta = seq - am[i]->cur_base_seq;
        } else {
          seq_delta = SEQ_MAX - am[i]->cur_base_seq + seq;
        }

        if(seq_delta < 0){   // this should never happen
          am[i]->base_seq_valid = 0;  // the seq number is not valid
          break;
        }

        if(seq_delta == 0){   // base_seq is not valid any more
          if ( len - am[i]->base_len != am[i]->flag[0]) { // lost the flag[0]
            am[i]->base_seq_valid = 0;  // the seq number is not valid
          }
          break;  // break any way for the next packet to come
        } 

        // delta is out of range, need to find the next flag[0] to start again
        if (seq_delta>=FLAG_NUM){
          am[i]->flag_match_num = 1;  // reset to 1
          if ( len - am[i]->base_len == am[i]->flag[0]) { // found the new flag[0]
            // here flag_match_num is already = 1
            am[i]->cur_base_seq = seq;
            am[i]->base_seq_valid = 1;  // the seq number is valid now
          } else {
            am[i]->base_seq_valid = 0;
          }
          break;    // done for this packet
        }

        // NODE_DBG("Smart: match_num:%d seq_delta:%d len:%d\n",am[i]->flag_match_num,seq_delta,len-am[i]->base_len);
        // seq_delta now from 1 to FLAG_NUM-1
        // flag[] == 0 ,means skip this flag.
        if ( (am[i]->flag_match_num==seq_delta) && \
          ( (am[i]->flag[am[i]->flag_match_num]==len-am[i]->base_len) || (am[i]->flag[am[i]->flag_match_num]==0) ) ){
          am[i]->flag_match_num++;
          if(am[i]->flag_match_num == FLAG_MATCH_NUM){  //every thing is match.
            NODE_ERR("Smart: got matched sender\n");
            matched = am[i];    // got the matched source-dest adress pair who is sending the udp data
            matched->base_seq_valid = 0;  // set to 0, and start to reference to the SSID_FLAG from now on
            os_timer_disarm(&smart_timer);  // note: may start a longer timeout
          }
          break;
        }

        // non match, reset, need to find next flag[0] to start again
        am[i]->flag_match_num = 1;
        am[i]->base_seq_valid = 0;
        break;
      } // non-match source-dest adress pair, continue to next pair in the map.
    } // for loop
    // break out, or loop done. 
    goto end;
  } else {  // cur_base_seq is ref to SSID_FLAG when patern is alread found
    if(0 != c_memcmp(matched->addr, &buf[ADDR_MATCH_START], ADDR_MATCH_LENGTH)){ // source-dest adress pair not match, ignore it
      return;
    }
    if (matched->base_seq_valid == 0){  // SSID_FLAG seq invalid, need to find the next valid seq number
    // base_seq not valid, find it
      if (len - matched->base_len == SSID_FLAG){
        matched->cur_base_seq = seq;
        matched->base_seq_valid = 1;
      } 
      goto end;
    }

    if(seq >= matched->cur_base_seq){
      seq_delta = seq - matched->cur_base_seq;
    } else {
      seq_delta = SEQ_MAX - matched->cur_base_seq + seq;
    }

    if(seq_delta < 0){   // this should never happen
      matched->base_seq_valid = 0;  // the seq number is not valid
      goto end;
    }

    if(seq_delta == 0){   // base_seq is not valid any more
      if ( len - matched->base_len != SSID_FLAG) { // lost the SSID_FLAG
        matched->base_seq_valid = 0;  // the seq number is not valid
      }
      goto end;  // exit for the next packet to come
    } 

    if ( seq_delta > (SEP_NUM + 1)*(1+NIBBLE_PER_BYTE*matched->ssid_len) +\
      1 + (SEP_NUM + 1)*(1+NIBBLE_PER_BYTE*matched->pwd_len) ){ 
    // delta out of the range
      if (len - matched->base_len == SSID_FLAG){
        matched->cur_base_seq = seq;
        matched->base_seq_valid = 1;
      } else {
        matched->base_seq_valid = 0;
      }
      goto end;
    }
        
    // delta in the range
    if (seq_delta==1){
      int16_t ssid_len = len - matched->base_len - L_FLAG;
      if ( matched->ssid_len == 0 ){   // update the ssid_len
        if ( (ssid_len <=32) && (ssid_len >0) ){
          matched->ssid_len = ssid_len;
          NODE_DBG("Smart: found the ssid_len %d\n", matched->ssid_len);
        }
        goto end;
      }
      if (ssid_len != matched->ssid_len){  // ssid_len not match
        matched->base_seq_valid = 0;
        // note: not match, save the new one or old one? for now save the new one.
        matched->ssid_len = ssid_len;  
        NODE_DBG("Smart: ssid_len not match\n");
      } 
      goto end; // to the next packet
    } 

    if( (SEP_NUM==2)&&(seq_delta==2 || seq_delta==3) ) {
      if (len - matched->base_len != matched->flag[seq_delta-2+SEP_1_INDEX]){  // SEP not match
        matched->base_seq_valid = 0;
        NODE_DBG("Smart: SEP-L not match\n");
      }
      goto end; // to the next packet
    }

    if( seq_delta==(SEP_NUM + 1)*(1+NIBBLE_PER_BYTE*matched->ssid_len) + 1) {
      if (len - matched->base_len != PWD_FLAG){  // PWD_FLAG not match
        matched->base_seq_valid = 0;
        NODE_DBG("Smart: PWD_FLAG not match\n");
      }
      goto end; // to the next packet
    }
        
    if (seq_delta==(SEP_NUM + 1)*(1+NIBBLE_PER_BYTE*matched->ssid_len) + 1 + 1){
      int16_t pwd_len = len - matched->base_len - L_FLAG;
      if ( matched->pwd_len == 0){
        if ( (pwd_len <=64) && (pwd_len>0)){
          matched->pwd_len = pwd_len;
          NODE_DBG("Smart: found the pwd_len %d\n", matched->pwd_len);
        }
        goto end; // to the next packet
      }
      if (pwd_len != matched->pwd_len){ // pwd_len not match
        matched->base_seq_valid = 0;
        // note: not match, save the new one or old one? for now save the new one.
        matched->pwd_len = pwd_len; // reset pwd_len to 0
        NODE_DBG("Smart: pwd_len not match\n");
      } 
      goto end;      
    } 

    if (seq_delta <= (SEP_NUM + 1)*(1+NIBBLE_PER_BYTE*matched->ssid_len) ){   // in the ssid zone
      uint16_t it = (seq_delta-1-SEP_NUM-1) / (SEP_NUM + 1);  // the number of ssid nibble: 0~31 or 0~63
      uint16_t m = (seq_delta-1-SEP_NUM-1) % (SEP_NUM + 1); // 0~2
      switch(m){
        case 0: // the ssid hi/lo-nibble itself    
          c = (int16_t)(len - matched->base_len - C_FLAG);
          if (c>255 || c<0){
            matched->base_seq_valid = 0;
            NODE_DBG("Smart: wrong ssid nibble\n");
            goto end;
          }
          byte_num = it / 8;  // 0~7
          bit_num = it % 8;   // 0~7
          if( (got_ssid[byte_num] & (0x1 << bit_num)) == 0){
            got_ssid[byte_num] |= 0x1 << bit_num; // set the bit
            ssid_nibble[it] = c;
          }           
          break;
        case 1: // seperator 1
        case 2: // seperator 2
          if(len - matched->base_len != matched->flag[m-1+SEP_1_INDEX]){
            NODE_DBG("Smart: SEP-S not match\n");
            matched->base_seq_valid = 0;
            goto end;
          }
          break;
        default:
          break;
      }
    } else {  // in the pwd zone
      uint16_t it = (seq_delta -1 -(SEP_NUM + 1)*(1+NIBBLE_PER_BYTE*matched->ssid_len) - 2 - SEP_NUM) / (SEP_NUM + 1); // the number of pwd byte
      uint16_t m = (seq_delta -1 -(SEP_NUM + 1)*(1+NIBBLE_PER_BYTE*matched->ssid_len) - 2 - SEP_NUM) % (SEP_NUM + 1);
      switch(m){
        case 0: // the pwd hi/lo-nibble itself
          c = (int16_t)(len - matched->base_len - C_FLAG);
          if (c>255 || c<0){
            matched->base_seq_valid = 0;
            NODE_DBG("Smart: wrong password nibble\n");
            goto end;
          }
          byte_num = it / 8;  // 0~15 / 7
          bit_num = it % 8;   // 0~7
          if( (got_password[byte_num] & (0x1 << bit_num)) == 0){
            got_password[byte_num] |= 0x1 << bit_num; // set the bit
            password_nibble[it] = c;
          } 
          break;
        case 1: // seperator 1
        case 2: // seperator 2
          if(len - matched->base_len != matched->flag[m-1+SEP_1_INDEX]){
            NODE_DBG("Smart: SEP-P not match\n");
            matched->base_seq_valid = 0;
            goto end;
          }
          break;
        default:
          break;
      }
    }
    // check if all done
    // NODE_DBG("Smart: ssid got %02x %02x\n", got_ssid[0], got_ssid[1]);
    // NODE_DBG("Smart: password got %02x %02x %02x\n", got_password[0], got_password[1], got_password[2]);
    int i,j;
    for(i=0;i<NIBBLE_PER_BYTE*matched->ssid_len;i++){
      byte_num = (i) / 8;
      bit_num = (i) % 8;
      if( (got_ssid[byte_num] & (0x1 << bit_num) ) != (0x1 << bit_num) ){ // check the bit == 1
        break;
      }
    }
    for(j=0;j<NIBBLE_PER_BYTE*matched->pwd_len;j++){
      byte_num = (j) / 8;
      bit_num = (j) % 8;
      if( (got_password[byte_num] & (0x1 << bit_num) ) != (0x1 << bit_num) ){ // check the 2 bit == 11
        break;
      }
    }
    if(matched->ssid_len > 0 && matched->pwd_len > 0 && i==NIBBLE_PER_BYTE*matched->ssid_len && j==NIBBLE_PER_BYTE*matched->pwd_len){    // get everything, check it.
      if( smart_check(ssid_nibble, NIBBLE_PER_BYTE*matched->ssid_len, sta_conf->ssid, got_ssid) && \
        smart_check(password_nibble, NIBBLE_PER_BYTE*matched->pwd_len, sta_conf->password, got_password) ){
        // all done
        alldone = 1;
        NODE_ERR(sta_conf->ssid);
        NODE_ERR(" %d\n", matched->ssid_len);
        NODE_ERR(sta_conf->password);
        NODE_ERR(" %d\n", matched->pwd_len);
        smart_end();
        // if(succeed){
        //   succeed(smart_succeed_arg);
        //   succeed = NULL; // reset to NULL when succeed
        //   smart_succeed_arg = NULL;
        // }
        return;
      }
    }
  }

end:
#if 0
  NODE_DBG("%d:\t0x%x 0x%x\t", len-BASE_LENGTH, buf[0],buf[1]);
  NODE_DBG(MACSTR, MAC2STR(&(buf[BSSID_ADDR])));
  NODE_DBG("\t");
  NODE_DBG(MACSTR, MAC2STR(&(buf[SOURCE_ADDR])));
  NODE_DBG("\t");
  NODE_DBG(MACSTR, MAC2STR(&(buf[DEST_ADDR])));
  uint16_t tseq = buf[SEQ_ADDR+1];
  tseq = tseq<<4;
  tseq += buf[SEQ_ADDR]>>4;
  NODE_DBG("\t0x%04x\n", tseq);
#endif
  return;
}

void reset_map(smart_addr_map **am, size_t num){
  int i;
  for (i = 0; i < num; ++i)
  {
    am[i]->flag_match_num = 0;
    am[i]->addr_len = ADDR_MATCH_LENGTH;
    am[i]->base_len = BASE_LENGTH;
    am[i]->cur_base_seq = -1;
    am[i]->base_seq_valid = 0;
    am[i]->ssid_len = 0;
    am[i]->pwd_len = 0;
    c_memset(am[i]->addr, 0, ADDR_MATCH_LENGTH);
    if(SEP_1_INDEX==0){
      am[i]->flag[0] = SEP_1;
      am[i]->flag[1] = SEP_2;
      am[i]->flag[2] = SSID_FLAG;
    }
    if(SEP_1_INDEX==2){
      am[i]->flag[0] = SSID_FLAG;
      am[i]->flag[1] = 0; // skip this flag
      am[i]->flag[2] = SEP_1;     
      am[i]->flag[3] = SEP_2;      
    }
  }
}

void smart_enable(void){
  wifi_promiscuous_enable(1); 
}

void smart_disable(void){
  wifi_promiscuous_enable(0); 
}

void smart_end(){
  int i;
  os_timer_disarm(&smart_timer);
  smart_disable();
  wifi_set_channel(cur_channel);

  if(NULL_MODE != mode){
    wifi_set_opmode(mode);
  } else {
    wifi_set_opmode(STATION_MODE);
  }
  
  mode = wifi_get_opmode();

  if(sta_conf && alldone){
    if( (STATION_MODE == mode) || (mode == STATIONAP_MODE) ){
      wifi_station_set_config(sta_conf);
      wifi_station_set_auto_connect(true);
      wifi_station_disconnect();
      wifi_station_connect();

      os_timer_disarm(&smart_timer);
      os_timer_setfn(&smart_timer, (os_timer_func_t *)station_check_connect, (void *)1);
      SWTIMER_REG_CB(station_check_connect, SWTIMER_RESUME);
        //the function station_check_connect continues the Smart config process and fires the developers callback upon successful connection to the access point.
        //If this function manages to get suspended, I think it would be fine to resume the timer.
      os_timer_arm(&smart_timer, STATION_CHECK_TIME, 0);   // no repeat
    }
  }

  for (i = 0; i < ADDR_MAP_NUM; ++i)
  {
    if(am[i]){
      c_free(am[i]);
      am[i] = NULL;
    }
    matched = NULL;
  }  

  if(sta_conf){
    c_free(sta_conf);
    sta_conf = NULL;
  }

  if(got_password){
    c_free(got_password);
    got_password = NULL;
  }

  if(got_ssid){
    c_free(got_ssid);
    got_ssid = NULL;
  }

  if(password_nibble){
    c_free(password_nibble);
    password_nibble = NULL;
  }

  if(ssid_nibble){
    c_free(ssid_nibble);
    ssid_nibble = NULL;
  }
  // system_restart();   // restart to enable the mode
}

void smart_next_channel(){
  smart_disable();
  switch(cur_channel){
    case 1:
      cur_channel = MAX_CHANNEL;
      break;
    case 2:
    case 3:
    case 4:
      cur_channel++;
      break;
    case 5:
      cur_channel = 7;
      break;
    case 6:
      cur_channel = 1;
      break;
    case 7:
    case 8:
    case 9:
    case 10:
    case 11:
    case 12:
      cur_channel++;
      break;
    case 13:
      cur_channel = 6;
      break;
    case MAX_CHANNEL:
      cur_channel = 2;
      break;
    default:
      cur_channel = 6;
      break;
  }

  NODE_ERR("switch to channel %d\n", cur_channel);
  wifi_set_channel(cur_channel);
  reset_map(am, ADDR_MAP_NUM);
  c_memset(sta_conf->ssid, 0, sizeof(sta_conf->ssid));
  c_memset(sta_conf->password, 0, sizeof(sta_conf->password));

  c_memset(got_ssid, 0, SSID_BIT_MAX);
  c_memset(got_password, 0, PWD_BIT_MAX);

  c_memset(ssid_nibble, 0, SSID_NIBBLE_MAX);
  c_memset(password_nibble, 0, PWD_NIBBLE_MAX);

  os_timer_disarm(&smart_timer);
  os_timer_arm(&smart_timer, TIME_OUT_PER_CHANNEL, 0);   // no repeat

  smart_enable();
}

void smart_begin(int chnl, smart_succeed s, void *arg){
  int i;
  alldone = 0;
  for (i = 0; i < ADDR_MAP_NUM; ++i)
  {
    if(!am[i]){
      am[i] = (smart_addr_map*)c_zalloc(sizeof(smart_addr_map));
      if(!am[i]){
        NODE_DBG("smart_begin map no memory\n");
        smart_end();
        return;
      }
    }
  }
  if(!sta_conf){
    sta_conf = (struct station_config *)c_zalloc(sizeof(struct station_config));
    if(!sta_conf){
      NODE_DBG("smart_begin sta_conf no memory\n");
      smart_end();
      return;
    }
  }

  if(!ssid_nibble){
    ssid_nibble = (uint8_t *)c_zalloc(SSID_NIBBLE_MAX);
    if(!ssid_nibble){
      NODE_DBG("smart_begin sta_conf no memory\n");
      smart_end();
      return;
    }
  }

  if(!password_nibble){
    password_nibble = (uint8_t *)c_zalloc(PWD_NIBBLE_MAX);
    if(!password_nibble){
      NODE_DBG("smart_begin sta_conf no memory\n");
      smart_end();
      return;
    }
  }

  if(!got_ssid){
    got_ssid = (uint8_t *)c_zalloc(SSID_BIT_MAX);
    if(!got_ssid){
      NODE_DBG("smart_begin sta_conf no memory\n");
      smart_end();
      return;
    }
  }

  if(!got_password){
    got_password = (uint8_t *)c_zalloc(PWD_BIT_MAX);
    if(!got_password){
      NODE_DBG("smart_begin sta_conf no memory\n");
      smart_end();
      return;
    }
  }
  reset_map(am, ADDR_MAP_NUM);
  // c_memset(sta_conf->ssid, 0, sizeof(sta_conf->ssid));
  // c_memset(sta_conf->password, 0, sizeof(sta_conf->password));

  // c_memset(got_ssid, 0, SSID_BIT_MAX);
  // c_memset(got_password, 0, PWD_BIT_MAX);

  // c_memset(ssid_nibble, 0, SSID_NIBBLE_MAX);
  // c_memset(password_nibble, 0, PWD_NIBBLE_MAX);
  mode = wifi_get_opmode();
  if( (STATION_MODE == mode) || (mode == STATIONAP_MODE) ){
    wifi_station_set_auto_connect(false);
    wifi_station_disconnect();
  }
  cur_channel = chnl;
  NODE_ERR("set channel to %d\n", cur_channel);
  wifi_set_channel(cur_channel);
  wifi_set_promiscuous_rx_cb(detect);
  os_timer_disarm(&smart_timer);
  os_timer_setfn(&smart_timer, (os_timer_func_t *)smart_next_channel, NULL);
  SWTIMER_REG_CB(smart_next_channel, SWTIMER_RESUME);
    //smart_next_channel switches the wifi channel
    //I don't see a problem with resuming this timer
  os_timer_arm(&smart_timer, TIME_OUT_PER_CHANNEL, 0);   // no repeat

  if(s){
    succeed = s;    // init the succeed call back
    smart_succeed_arg = arg;
  }

  smart_enable();
}

void station_check_connect(bool smart){
  mode = wifi_get_opmode();
  if( (STATION_MODE != mode) && (mode != STATIONAP_MODE) ){
    return;
  }
  uint8_t status = wifi_station_get_connect_status();
  switch(status){
    case STATION_GOT_IP:
      NODE_DBG("station_check_connect is called with status: GOT_IP\n");
      if(succeed){
        succeed(smart_succeed_arg);
        succeed = NULL; // reset to NULL when succeed
        smart_succeed_arg = NULL;
      }
      return;
    case STATION_CONNECTING:
      NODE_DBG("station_check_connect is called with status: CONNECTING\n");
      break;
    case STATION_IDLE:
      wifi_station_set_auto_connect(true);
    case STATION_CONNECT_FAIL:
    case STATION_NO_AP_FOUND:
      wifi_station_disconnect();
      wifi_station_connect();
      NODE_DBG("station_check_connect is called with smart: %d\n", smart);
      break;
    case STATION_WRONG_PASSWORD:
      if(smart)
        smart_begin(cur_channel, succeed, smart_succeed_arg);
      return;
    default:
      break;
  }
  os_timer_disarm(&smart_timer);
  os_timer_setfn(&smart_timer, (os_timer_func_t *)station_check_connect, (void *)(int)smart);
    //this function was already registered in the function smart_end.
  os_timer_arm(&smart_timer, STATION_CHECK_TIME, 0);   // no repeat
}
