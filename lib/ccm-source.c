/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * ccm-source.c
 * Copyright (C) Nicolas Bruguier 2010 <gandalfn@club-internet.fr>
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

#include "ccm-source.h"

struct _CCMSource
{
    GSource m_Source;
    CCMSourceFuncs m_Funcs;
    gpointer m_Data;
};

static gboolean
ccm_source_prepare (GSource* inSource, gint* outTimeout)
{
    CCMSource* source = (CCMSource*) inSource;

    *outTimeout = -1;

    if (source->m_Funcs.prepare)
        return source->m_Funcs.prepare(source->m_Data, outTimeout);
    else
        return FALSE;
}

static gboolean
ccm_source_check (GSource* inSource)
{
    CCMSource* source = (CCMSource*) inSource;

    if (source->m_Funcs.check)
        return source->m_Funcs.check(source->m_Data);
    else
        return FALSE;
}

static gboolean
ccm_source_dispatch (GSource* inSource, GSourceFunc inCallback, gpointer inUserData)
{
    CCMSource* source = (CCMSource*) inSource;
    gboolean ret = FALSE;

    g_source_ref (inSource);
    if (source->m_Funcs.dispatch)
        ret = source->m_Funcs.dispatch(source->m_Data, inCallback, inUserData);
    g_source_unref (inSource);

    return ret;
}

static void
ccm_source_finalize (GSource* inSource)
{
    CCMSource* source = (CCMSource*) inSource;

    if (source->m_Funcs.finalize)
        source->m_Funcs.finalize(source->m_Data);
}

static GSourceFuncs s_CCMSourceFuncs = {
    ccm_source_prepare,
    ccm_source_check,
    ccm_source_dispatch,
    ccm_source_finalize,
};

CCMSource* 
ccm_source_new (CCMSourceFuncs inFuncs, gpointer inData) 
{
    g_return_val_if_fail(inData != NULL, NULL);

    CCMSource* self;

    self = (CCMSource*)g_source_new (&s_CCMSourceFuncs, sizeof (CCMSource));
    self->m_Funcs = inFuncs;
    self->m_Data = inData;

    return self;
}


CCMSource* 
ccm_source_new_from_pollfd (CCMSourceFuncs inFuncs, GPollFD* inpFd,
                            gpointer inData) 
{
    g_return_val_if_fail(inpFd != NULL, NULL);
    g_return_val_if_fail(inData != NULL, NULL);

    CCMSource* self;

    self = (CCMSource*)g_source_new (&s_CCMSourceFuncs, sizeof (CCMSource));
    self->m_Funcs = inFuncs;
    self->m_Data = inData;
    g_source_add_poll ((GSource*) self, inpFd);
    g_source_set_can_recurse ((GSource*) self, TRUE);

    return self;
}

CCMSource*
ccm_source_ref (CCMSource* self)
{
    g_return_val_if_fail(self != NULL, NULL);

    g_source_ref ((GSource*)self);

    return self;
}

void
ccm_source_unref (CCMSource* self)
{
    g_return_if_fail(self != NULL);

    g_source_unref ((GSource*)self);
}

void
ccm_source_destroy (CCMSource* self)
{
    g_return_if_fail(self != NULL);

    g_source_destroy ((GSource*)self);
}
