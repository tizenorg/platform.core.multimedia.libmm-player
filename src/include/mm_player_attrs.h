/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, YeJin Cho <cho.yejin@samsung.com>,
 * Seungbae Shin <seungbae.shin@samsung.com>, YoungHwan An <younghwan_.an@samsung.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifndef __MM_PLAYER_ATTRS_H__
#define	__MM_PLAYER_ATTRS_H__

#ifdef __cplusplus
	extern "C" {
#endif

int _mmplayer_set_attribute(MMHandleType player,  char **err_atr_name, const char *attribute_name, va_list args_list);

int _mmplayer_get_attribute(MMHandleType player,  char **err_atr_name, const char *attribute_name, va_list args_list);

int _mmplayer_get_attribute_info(MMHandleType player,  const char *attribute_name, MMPlayerAttrsInfo *info);

bool _mmplayer_construct_attribute(mm_player_t* player);

void _mmplayer_release_attrs(mm_player_t* player);

#ifdef __cplusplus
	}
#endif

#endif /* __MM_PLAYER_ATTRS_H__ */
