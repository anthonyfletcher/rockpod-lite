/***************************************************************************
 * RockPod-Lite
 *
 * Original code from RockBox
 * Copyright (C) 2002 by Alan Korr & Nick Robinson
 *
 * All files in this archive are subject to the GNU General Public License.
 * See the file COPYING in the source tree root for full license agreement.
 * GNU General Public License (version 2+)
 *
 * Declares the per-lingo packet handlers.
 ****************************************************************************/

void iap_handlepkt_mode0(const unsigned int len, const unsigned char *buf);
void iap_handlepkt_mode2(const unsigned int len, const unsigned char *buf);
void iap_handlepkt_mode3(const unsigned int len, const unsigned char *buf);
void iap_handlepkt_mode4(const unsigned int len, const unsigned char *buf);
