#include <algorithm>
#include <imi_view.h>
#include <imi_options.h>
#include <imi_keys.h>

#include "sunpinyin_property.h"
#include "sunpinyin_lookup_table.h"
#include "imi_ibus_win.h"

#include "sunpinyin_engine.h"

using namespace std;

SunPinyinEngine::SunPinyinEngine()
    : m_status_prop(NULL),
      m_letter_prop(NULL),
      m_punct_prop(NULL),
      m_shuangpin_prop(NULL),
      m_prop_list(NULL),
      m_lookup_table(NULL),
      m_parent(NULL)
{}

SunPinyinEngine::~SunPinyinEngine()
{}

void
SunPinyinEngine::init ()
{
    m_prop_list = ibus_prop_list_new();
    
    m_status_prop = SunPinyinProperty::create_status_prop(this);
    ibus_prop_list_append(m_prop_list, m_status_prop->get());
    
    m_letter_prop = SunPinyinProperty::create_letter_prop(this);
    ibus_prop_list_append(m_prop_list, m_letter_prop->get());
    
    m_punct_prop = SunPinyinProperty::create_punct_prop(this);
    ibus_prop_list_append(m_prop_list, m_punct_prop->get());
    
    m_shuangpin_prop = SunPinyinProperty::create_shuangpin_prop(this);
    ibus_prop_list_append(m_prop_list, m_shuangpin_prop->get());
    
    m_lookup_table = new SunPinyinLookupTable();

    CSunpinyinSessionFactory& factory = CSunpinyinSessionFactory::getFactory();
    m_pv = factory.createSession();
    if (!m_pv)
        return;
    m_wh = new CIBusWinHandler(this);
    m_pv->attachWinHandler(m_wh);
}

void
SunPinyinEngine::destroy ()
{
    delete m_lookup_table;
    m_lookup_table = NULL;
    
    if (m_prop_list) {
        g_object_unref (m_prop_list);
        m_prop_list = NULL;
    }
    delete m_status_prop;
    m_status_prop = NULL;
    
    delete m_letter_prop;
    m_letter_prop = NULL;

    delete m_punct_prop;
    m_punct_prop = NULL;

    delete m_shuangpin_prop;
    m_shuangpin_prop = NULL;

    if (m_pv) {
        CSunpinyinSessionFactory& factory = CSunpinyinSessionFactory::getFactory();
        factory.destroySession(m_pv);
        m_pv = NULL;
    }
    
    delete m_wh;
    m_wh = NULL;
    
    IBUS_OBJECT_CLASS (m_parent)->destroy ((IBusObject *)this);
}

void
SunPinyinEngine::set_parent_class(IBusEngineClass *klass)
{
    m_parent = klass;
}

gboolean
SunPinyinEngine::process_key_event (guint key_val,
                                    guint key_code,
                                    guint modifiers)
{
    if (!is_valid()) return FALSE;
    
    if (modifiers & IBUS_RELEASE_MASK)
        return FALSE;
    return (try_switch_cn(key_val, key_code, modifiers) ||
            try_process_key(key_val, key_code, modifiers));
}

gboolean
SunPinyinEngine::try_switch_cn (guint key_val,
                                guint key_code,
                                guint modifiers)
{
    if ((key_code == IBUS_Shift_L &&
         modifiers == (IBUS_SHIFT_MASK|IBUS_RELEASE_MASK)) ||
        (key_code == IBUS_Shift_R &&
         modifiers == (IBUS_SHIFT_MASK|IBUS_RELEASE_MASK))) {
        property_activate(m_status_prop->name(), !m_status_prop->state());
        reset();
        return TRUE;
    }
    return FALSE;
}

static CKeyEvent
translate_key(const CKeyEvent& k)
{
    if (k.value == IM_VK_SPACE && k.code == 0x39) {
        return CKeyEvent(IM_VK_SPACE, IM_VK_SPACE);
    }
    if (k.value == IM_VK_HOME && k.code == 0x66) {
        return CKeyEvent(IM_VK_HOME, IM_VK_HOME);
    }
    if (k.value == IM_VK_END && k.code == 0x66) {
        return CKeyEvent(IM_VK_END, IM_VK_END);
    }
    if (k.value == IM_VK_PAGE_UP ||
        k.value == IM_VK_PAGE_DOWN ||
        k.value == IM_VK_DELETE ||
        k.value == IM_VK_BACK_SPACE ||
        k.value == IM_VK_ENTER || 
        k.value == IM_VK_ESCAPE) {
        // swap the keyval and keycode
        return CKeyEvent(k.value, k.code, k.modifiers);
    }
    return k;
}


gboolean
SunPinyinEngine::try_process_key (guint key_val,
                                  guint key_code,
                                  guint modifiers)
{
    CKeyEvent key(key_code, key_val, modifiers);
    key = translate_key(key);
    return m_pv->onKeyEvent(key) ? TRUE : FALSE;
}

void
SunPinyinEngine::focus_in ()
{
    if (!is_valid()) return;
    
    ibus_engine_register_properties(this, m_prop_list);
    m_pv->updateWindows(CIMIView::PREEDIT_MASK | CIMIView::CANDIDATE_MASK);
    m_parent->focus_in(this);
}

void
SunPinyinEngine::focus_out ()
{
    if (!is_valid()) return;
    
    reset();
    m_parent->focus_out(this);
}

void
SunPinyinEngine::reset ()
{
    if (!is_valid()) return;
    
    m_pv->updateWindows(m_pv->clearIC());
    m_parent->reset(this);
}

void
SunPinyinEngine::enable ()
{
    if (!is_valid()) return;
    
    focus_in();
    m_parent->enable(this);
}

void
SunPinyinEngine::disable ()
{
    if (!is_valid()) return;
    
    m_parent->disable(this);
}

void
SunPinyinEngine::page_up ()
{
    if (!is_valid()) return;
    
    m_pv->onCandidatePageRequest(-1, true /* relative */);
    m_parent->page_up(this);
}

void
SunPinyinEngine::page_down ()
{
    if (!is_valid()) return;
    
    m_pv->onCandidatePageRequest(1, true /* relative */);
    m_parent->page_down(this);
}

void
SunPinyinEngine::property_activate (const std::string& property, unsigned state)
{
    if (!is_valid()) return;
    
    if (m_status_prop->update(property, state)) {
        m_pv->setStatusAttrValue(CIMIWinHandler::STATUS_ID_CN, state);
    } else if (m_letter_prop->update(property, state)) {
        m_pv->setStatusAttrValue(CIMIWinHandler::STATUS_ID_FULLSYMBOL, state);
    } else if (m_punct_prop->update(property, state)) {
        m_pv->setStatusAttrValue(CIMIWinHandler::STATUS_ID_FULLPUNC, state);
    }
    // TODO: shuangpin
    m_parent->property_activate(this, property.c_str(), state);
}

void
SunPinyinEngine::candidate_clicked (guint index)
{
    m_pv->onCandidateSelectRequest(index);
}

void
SunPinyinEngine::cursor_up ()
{
    if (!is_valid()) return;
    
    if (m_lookup_table->cursor_up()) {
        update_lookup_table();
        m_parent->cursor_up(this);
    }
}

void
SunPinyinEngine::cursor_down ()
{
    if (!is_valid()) return;
    
    if (m_lookup_table->cursor_down()) {
        update_lookup_table();
        m_parent->cursor_down(this);
    }
}

void
SunPinyinEngine::commit_string (const std::wstring& str)
{
    IBusText *text;
    text = ibus_text_new_from_ucs4((const gunichar*) str.c_str());
    ibus_engine_commit_text(this, text);
    g_object_unref(text);
}

void
SunPinyinEngine::update_candidates(const ICandidateList& cl)
{
    if (m_lookup_table->update_candidates(cl) > 0)
        update_lookup_table();
    else
        ibus_engine_hide_lookup_table (this);
}

void
SunPinyinEngine::update_lookup_table()
{
    ibus_engine_update_lookup_table(this, m_lookup_table->get(), TRUE);
}

bool
SunPinyinEngine::is_valid() const
{
    return m_pv != NULL;
}

void
SunPinyinEngine::update_preedit_string(const IPreeditString& preedit)
{
    const int len = preedit.size();
    if (len > 0) {
        IBusText *text = ibus_text_new_from_ucs4((const gunichar*) preedit.string());
        const int caret = preedit.caret();
        if (caret > 0 && caret <= len) {
            // decorate preedit string by charTypeAt(idx)
            int candi_end = preedit.candi_start()+preedit.charTypeSize();
            ibus_text_append_attribute (text,
                                        IBUS_ATTR_TYPE_FOREGROUND,
                                        0x00000000,
                                        preedit.candi_start(),
                                        candi_end);
        }
        ibus_engine_update_preedit_text(this, text, preedit.caret(), TRUE);
        g_object_unref(text);
    } else {
        ibus_engine_hide_preedit_text(this);
    }
}

void
SunPinyinEngine::update_status_property(bool cn)
{
    m_status_prop->update(cn);
}

void
SunPinyinEngine::update_punct_property(bool full)
{
    m_punct_prop->update(full);
}

void
SunPinyinEngine::update_letter_property(bool full)
{
    m_letter_prop->update(full);
}
