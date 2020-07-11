/*
  +----------------------------------------------------------------------+
  | Swoole                                                               |
  +----------------------------------------------------------------------+
  | This source file is subject to version 2.0 of the Apache license,    |
  | that is bundled with this package in the file LICENSE, and is        |
  | available through the world-wide-web at the following url:           |
  | http://www.apache.org/licenses/LICENSE-2.0.html                      |
  | If you did not receive a copy of the Apache2.0 license and are unable|
  | to obtain it through the world-wide-web, please send a note to       |
  | license@swoole.com so we can mail you a copy immediately.            |
  +----------------------------------------------------------------------+
  | Author: Tianfeng Han  <mikan.tenny@gmail.com>                        |
  +----------------------------------------------------------------------+
*/

#include "php_swoole_async.h"
#include "ext/swoole/include/msg_queue.h"

static PHP_METHOD(swoole_msgqueue, __construct);
static PHP_METHOD(swoole_msgqueue, __destruct);
static PHP_METHOD(swoole_msgqueue, push);
static PHP_METHOD(swoole_msgqueue, pop);
static PHP_METHOD(swoole_msgqueue, setBlocking);
static PHP_METHOD(swoole_msgqueue, stats);
static PHP_METHOD(swoole_msgqueue, destroy);

zend_class_entry *swoole_msgqueue_ce;
static zend_object_handlers swoole_msgqueue_handlers;

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_msgqueue_construct, 0, 0, 1)
    ZEND_ARG_INFO(0, len)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_msgqueue_push, 0, 0, 1)
    ZEND_ARG_INFO(0, data)
    ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_msgqueue_pop, 0, 0, 0)
    ZEND_ARG_INFO(0, type)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_msgqueue_setBlocking, 0, 0, 1)
    ZEND_ARG_INFO(0, blocking)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(arginfo_swoole_void, 0, 0, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry swoole_msgqueue_methods[] =
{
    PHP_ME(swoole_msgqueue, __construct, arginfo_swoole_msgqueue_construct, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_msgqueue, __destruct, arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_msgqueue, push, arginfo_swoole_msgqueue_push, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_msgqueue, pop, arginfo_swoole_msgqueue_pop, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_msgqueue, setBlocking, arginfo_swoole_msgqueue_setBlocking, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_msgqueue, stats, arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_ME(swoole_msgqueue, destroy, arginfo_swoole_void, ZEND_ACC_PUBLIC)
    PHP_FE_END
};

void swoole_msgqueue_init(int module_number)
{
    SW_INIT_CLASS_ENTRY(swoole_msgqueue, "Swoole\\MsgQueue", "swoole_msgqueue", NULL, swoole_msgqueue_methods);
    SW_SET_CLASS_SERIALIZABLE(swoole_msgqueue, zend_class_serialize_deny, zend_class_unserialize_deny);
    SW_SET_CLASS_CLONEABLE(swoole_msgqueue, sw_zend_class_clone_deny);
    SW_SET_CLASS_UNSET_PROPERTY_HANDLER(swoole_msgqueue, sw_zend_class_unset_property_deny);
}

static PHP_METHOD(swoole_msgqueue, __construct)
{
    zend_long key;
    zend_long perms = 0;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "l|l", &key, &perms) == FAILURE)
    {
        RETURN_FALSE;
    }

    swMsgQueue *queue = (swMsgQueue *) emalloc(sizeof(swMsgQueue));
    if (queue == NULL)
    {
        zend_throw_exception(swoole_exception_ce, "failed to create MsgQueue.", SW_ERROR_MALLOC_FAIL);
        RETURN_FALSE;
    }
    if (swMsgQueue_create(queue, 1, key, perms))
    {
        zend_throw_exception(swoole_exception_ce, "failed to init MsgQueue.", SW_ERROR_MALLOC_FAIL);
        RETURN_FALSE;
    }
    swoole_set_object(ZEND_THIS, queue);
}

static PHP_METHOD(swoole_msgqueue, __destruct)
{
    SW_PREVENT_USER_DESTRUCT();

    swMsgQueue *queue = (swMsgQueue *) swoole_get_object(ZEND_THIS);
    efree(queue);
    swoole_set_object(ZEND_THIS, NULL);
}

static PHP_METHOD(swoole_msgqueue, push)
{
    char *data;
    size_t length;
    zend_long type = 1;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "s|l", &data, &length, &type) == FAILURE)
    {
        RETURN_FALSE;
    }

    swQueue_data *in = (swQueue_data *) emalloc(length + sizeof(long) + 1);
    in->mtype = type;
    memcpy(in->mdata, data, length + 1);

    swMsgQueue *queue = (swMsgQueue *) swoole_get_object(ZEND_THIS);
    int ret = swMsgQueue_push(queue, in, length);
    efree(in);
    SW_CHECK_RETURN(ret);
}

static PHP_METHOD(swoole_msgqueue, pop)
{
    long type = 1;
    swQueue_data out;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "|l", &type) == FAILURE)
    {
        RETURN_FALSE;
    }

    swMsgQueue *queue = (swMsgQueue *) swoole_get_object(ZEND_THIS);
    out.mtype = type;
    int length = swMsgQueue_pop(queue, &out, sizeof(out.mdata));
    if (length < 0)
    {
        RETURN_FALSE;
    }
    RETURN_STRINGL(out.mdata, length);
}

static PHP_METHOD(swoole_msgqueue, setBlocking)
{
    swMsgQueue *queue = (swMsgQueue *) swoole_get_object(ZEND_THIS);
    zend_bool blocking;

    if (zend_parse_parameters(ZEND_NUM_ARGS(), "b", &blocking) == FAILURE)
    {
        RETURN_FALSE;
    }
    swMsgQueue_set_blocking(queue, blocking);
}

static PHP_METHOD(swoole_msgqueue, stats)
{
    swMsgQueue *queue = (swMsgQueue *) swoole_get_object(ZEND_THIS);
    int queue_num = -1;
    int queue_bytes = -1;
    if (swMsgQueue_stat(queue, &queue_num, &queue_bytes) == 0)
    {
        array_init(return_value);
        add_assoc_long_ex(return_value, ZEND_STRL("queue_num"), queue_num);
        add_assoc_long_ex(return_value, ZEND_STRL("queue_bytes"), queue_bytes);
    }
    else
    {
        RETURN_FALSE;
    }
}

static PHP_METHOD(swoole_msgqueue, destroy)
{
    swMsgQueue *queue = (swMsgQueue *) swoole_get_object(ZEND_THIS);
    SW_CHECK_RETURN(swMsgQueue_free(queue));
}
