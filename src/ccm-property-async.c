/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * cairo-compmgr
 * Copyright (C) Nicolas Bruguier 2008 <gandalfn@club-internet.fr>
 * 
 * cairo-compmgr is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * cairo-compmgr is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <X11/Xlibint.h>

#include "ccm-debug.h"
#include "ccm-property-async.h"

enum
{
	REPLY,
    N_SIGNALS
};

static guint signals[N_SIGNALS] = { 0 };

struct _CCMPropertyASyncPrivate
{
	CCMDisplay* 	display;
	Window			window;
	
	Atom			property;
	_XAsyncHandler  async;
	gulong		 	request_seq;
	
	gchar*			data;
	gulong			n_items;
};

#define CCM_PROPERTY_ASYNC_GET_PRIVATE(o)  \
   ((CCMPropertyASyncPrivate*)G_TYPE_INSTANCE_GET_PRIVATE ((o), CCM_TYPE_PROPERTY_ASYNC, CCMPropertyASyncClass))

G_DEFINE_TYPE (CCMPropertyASync, ccm_property_async, G_TYPE_OBJECT);

static void
ccm_property_async_init (CCMPropertyASync *self)
{
	self->priv = CCM_PROPERTY_ASYNC_GET_PRIVATE(self);
	
	self->priv->display = NULL;
	self->priv->property = None;
	self->priv->request_seq = 0;
	self->priv->data = NULL;
	self->priv->n_items = 0;
}

static void
ccm_property_async_finalize (GObject *object)
{
	CCMPropertyASync* self = CCM_PROPERTY_ASYNC(object);
	
	if (self->priv->data)
	{
		XFree(self->priv->data);
		self->priv->data = NULL;
	}
	self->priv->n_items = 0;
	if (self->priv->display)
	{
		DeqAsyncHandler (CCM_DISPLAY_XDISPLAY(self->priv->display), 
						 &self->priv->async);
		self->priv->display = NULL;
	}
	self->priv->request_seq = 0;
		
	G_OBJECT_CLASS (ccm_property_async_parent_class)->finalize (object);
}

static void
ccm_property_async_class_init (CCMPropertyASyncClass *klass)
{
	GObjectClass* object_class = G_OBJECT_CLASS (klass);
	
	g_type_class_add_private (klass, sizeof (CCMPropertyASyncPrivate));

	signals[REPLY] = g_signal_new ("reply",
								   G_OBJECT_CLASS_TYPE (object_class),
								   G_SIGNAL_RUN_LAST, 0, NULL, NULL,
								   g_cclosure_marshal_VOID__UINT_POINTER,
								   G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);
	
	object_class->finalize = ccm_property_async_finalize;
}

#define ALIGN_VALUE(this, boundary) \
  (( ((unsigned long)(this)) + (((unsigned long)(boundary)) -1)) & (~(((unsigned long)(boundary))-1)))

static gboolean
ccm_property_async_idle(CCMPropertyASync* self)
{
	g_return_val_if_fail(self != NULL, FALSE);
	
	ccm_debug("IDLE DATA %i %x", self->priv->n_items, self->priv->data);
	
	g_signal_emit (self, signals[REPLY], 0, 
				   self->priv->n_items, self->priv->data);
	
	return FALSE;
}

static Bool
ccm_property_async_handler (Display *dpy, xReply *rep, char *buf,
                            int len, XPointer dta)
{
	CCMPropertyASync* self = CCM_PROPERTY_ASYNC(dta);
	xGetPropertyReply replbuf, *reply;
	
	if (dpy->last_request_read != self->priv->request_seq)
		return False;
	
	if (rep->generic.type == X_Error)
		return False;
	
	reply = (xGetPropertyReply *)
		    _XGetAsyncReply(dpy, (char *)&replbuf, rep, buf, len,
							(sizeof(xGetPropertyReply) - sizeof(xReply)) >> 2,
							True);
	ccm_debug("ASYNC PROPERTY 0x%lx: %i", self->priv->window, self->priv->request_seq);
	
	if (reply->propertyType != None)
	{
		gulong nbytes, netbytes;
		
		switch (reply->format)
        {
			case 8:
				nbytes = reply->nItems;
				netbytes = ALIGN_VALUE (nbytes, 4);
				if (nbytes + 1 > 0 &&
					(self->priv->data = (gchar *) Xmalloc ((unsigned)nbytes + 1)))
				{
					_XGetAsyncData (dpy, self->priv->data, buf, len,
									sizeof(xGetPropertyReply), nbytes,
									netbytes);
				}
          	break;
        	case 16:
          		nbytes = reply->nItems * sizeof (short);
          		netbytes = reply->nItems << 1;
          		netbytes = ALIGN_VALUE (netbytes, 4);
          		if (nbytes + 1 > 0 &&
					(self->priv->data = (gchar *) Xmalloc ((unsigned)nbytes + 1)))
            	{
					_XGetAsyncData (dpy, self->priv->data, buf, len,
									sizeof(xGetPropertyReply), nbytes, 
									netbytes);
            	}
			break;
        	case 32:
				nbytes = reply->nItems * sizeof (long);
          		netbytes = reply->nItems << 2;
          		if (nbytes + 1 > 0 &&
					(self->priv->data = (gchar *) Xmalloc ((unsigned)nbytes + 1)))
				{
              		if (sizeof (long) == 8)
                	{
                  		char *netdata;
                  		char *lptr;
                  		char *end_lptr;
                  
						netdata = self->priv->data + nbytes / 2;

                  		_XGetAsyncData (dpy, netdata, buf, len,
										sizeof(xGetPropertyReply), netbytes,
										netbytes);
						
						lptr = self->priv->data;
						end_lptr = self->priv->data + nbytes;
						while (lptr != end_lptr)
						{
							*(long*) lptr = *(CARD32*) netdata;
							lptr += sizeof (long);
							netdata += sizeof (CARD32);
						}
					}
					else
					{
						_XGetAsyncData (dpy, self->priv->data, buf, len,
										sizeof(xGetPropertyReply), netbytes,
										netbytes);
                	}
            	}
          	break;
	        default:
				ccm_debug("INVALID TYPE");
				nbytes = netbytes = 0L;
          	break;
        }
	
		if (self->priv->data)
		{
			self->priv->n_items = reply->nItems;
			ccm_debug("DATA %i", self->priv->n_items);
			g_idle_add_full (G_PRIORITY_HIGH, 
							 (GSourceFunc)ccm_property_async_idle, self, NULL);
		}
		else
		{
			ccm_debug("BAD ALLOC");
			_XGetAsyncData (dpy, NULL, buf, len,
							sizeof(xGetPropertyReply), 0, netbytes);
			return BadAlloc;
		}
	}
	
	return True;
}

CCMPropertyASync*
ccm_property_async_new(CCMDisplay* display, Window window, Atom property,
					   Atom req_type, long length)
{
	g_return_val_if_fail(display != NULL, NULL);
	g_return_val_if_fail(window != None, NULL);
	
	CCMPropertyASync* self = g_object_new(CCM_TYPE_PROPERTY_ASYNC, NULL);
	Display* dpy = CCM_DISPLAY_XDISPLAY(display);
	xGetPropertyReq *req;
	
	self->priv->display = display;
	self->priv->window = window;
	self->priv->property = property;
	
	LockDisplay (dpy);
	
	self->priv->async.next = dpy->async_handlers;
  	self->priv->async.handler = ccm_property_async_handler;
  	self->priv->async.data = (XPointer) self;
  	dpy->async_handlers = &self->priv->async;
	
	GetReq (GetProperty, req);
  	self->priv->request_seq = dpy->request;
	ccm_debug("GET ASYNC PROPERTY 0x%lx: %i", self->priv->window, self->priv->request_seq);
	req->window = window;
  	req->property = property;
  	req->type = req_type;
  	req->delete = False;
  	req->longOffset = 0;
  	req->longLength = length;
	
	UnlockDisplay (dpy);
	SyncHandle ();
	
	return self;
}

Atom
ccm_property_async_get_property(CCMPropertyASync* self)
{
	g_return_val_if_fail(self != NULL, None);
	
	return self->priv ? self->priv->property : None;
}
