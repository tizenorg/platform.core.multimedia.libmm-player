/*
 * libmm-player
 *
 * Copyright (c) 2000 - 2011 Samsung Electronics Co., Ltd. All rights reserved.
 *
 * Contact: JongHyuk Choi <jhchoi.choi@samsung.com>, YeJin Cho <cho.yejin@samsung.com>, YoungHwan An <younghwan_.an@samsung.com>
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

/**
 * This function set values of attributes.
 *
 * @param[in]	handle			Handle of player.
 * @param[in]	err_atr_name		Name of attribute that is failed (NULL can be set if it's not require to check. )
 * @param[in]	attribute_name	Name of the first attribute to set
 * @param[in]	args_list			List of attributes and values
 * @return	This function returns zero on success, or negative value with error code.
 * @remarks
 * @see		_mmplayer_get_attribute()
 *
 */
int _mmplayer_set_attribute(MMHandleType handle,  char **err_atr_name, const char *attribute_name, va_list args_list);
/**
 * This function get values of attributes.
 *
 * @param[in]	handle			Handle of player.
 * @param[in]	err_atr_name		Name of attribute that is failed (NULL can be set if it's not require to check. )
 * @param[in]	attribute_name	Name of the first attribute to set
 * @param[in]	args_list			List of attributes and values
 * @return	This function returns zero on success, or negative value with error code.
 * @remarks
 * @see		_mmplayer_set_attribute()
 *
 */
int _mmplayer_get_attribute(MMHandleType handle,  char **err_atr_name, const char *attribute_name, va_list args_list);
/**
 * This function get configuration values of attribute.
 *
 * @param[in]	handle			Handle of player.
 * @param[in]	attribute_name	Name of the first attribute to set
 * @param[in]	info				Configuration values
 * @return	This function returns zero on success, or negative value with error code.
 * @remarks
 * @see
 *
 */
int _mmplayer_get_attribute_info(MMHandleType handle,  const char *attribute_name, MMPlayerAttrsInfo *info);
/**
 * This function allocates structure of attributes and sets initial values.
 *
 * @param[in]	handle		Handle of player.
 * @return	This function returns allocated structure of attributes.
 * @remarks
 * @see		_mmplayer_deconstruct_attribute()
 *
 */
MMHandleType _mmplayer_construct_attribute(MMHandleType handle);
/**
 * This function release allocated attributes.
 *
 * @param[in]	handle		Handle of player.
 * @return	This function returns true on success or false on failure.
 * @remarks
 * @see		_mmplayer_construct_attribute()
 *
 */
bool _mmplayer_deconstruct_attribute(MMHandleType handle);

#ifdef __cplusplus
	}
#endif

#endif /* __MM_PLAYER_ATTRS_H__ */
