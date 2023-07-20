# **********************************************************************
# * Copyright (C) 2022 MX Authors
# *
# * Authors: Adrian
# *          MX Linux <http://mxlinux.org>
# *
# * This is free software: you can redistribute it and/or modify
# * it under the terms of the GNU General Public License as published by
# * the Free Software Foundation, either version 3 of the License, or
# * (at your option) any later version.
# *
# * This program is distributed in the hope that it will be useful,
# * but WITHOUT ANY WARRANTY; without even the implied warranty of
# * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# * GNU General Public License for more details.
# *
# * You should have received a copy of the GNU General Public License
# * along with this package. If not, see <http://www.gnu.org/licenses/>.
# **********************************************************************/

QT       += core gui widgets
CONFIG   += c++1z

TARGET = deb-installer
TEMPLATE = app

# The following define makes your compiler warn you if you use any
# feature of Qt which has been marked as deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

SOURCES += main.cpp\
    cmd.cpp \
    installer.cpp

HEADERS  += \
    installer.h \
    version.h \
    cmd.h

FORMS    +=

TRANSLATIONS += translations/deb-installer_af.ts \
                translations/deb-installer_am.ts \
                translations/deb-installer_ar.ts \
                translations/deb-installer_be.ts \
                translations/deb-installer_bg.ts \
                translations/deb-installer_bn.ts \
                translations/deb-installer_bs_BA.ts \
                translations/deb-installer_bs.ts \
                translations/deb-installer_ca.ts \
                translations/deb-installer_ceb.ts \
                translations/deb-installer_co.ts \
                translations/deb-installer_cs.ts \
                translations/deb-installer_cy.ts \
                translations/deb-installer_da.ts \
                translations/deb-installer_de.ts \
                translations/deb-installer_el.ts \
                translations/deb-installer_en_GB.ts \
                translations/deb-installer_en.ts \
                translations/deb-installer_en_US.ts \
                translations/deb-installer_eo.ts \
                translations/deb-installer_es_ES.ts \
                translations/deb-installer_es.ts \
                translations/deb-installer_et.ts \
                translations/deb-installer_eu.ts \
                translations/deb-installer_fa.ts \
                translations/deb-installer_fi_FI.ts \
                translations/deb-installer_fil_PH.ts \
                translations/deb-installer_fil.ts \
                translations/deb-installer_fi.ts \
                translations/deb-installer_fr_BE.ts \
                translations/deb-installer_fr.ts \
                translations/deb-installer_fy.ts \
                translations/deb-installer_ga.ts \
                translations/deb-installer_gd.ts \
                translations/deb-installer_gl_ES.ts \
                translations/deb-installer_gl.ts \
                translations/deb-installer_gu.ts \
                translations/deb-installer_ha.ts \
                translations/deb-installer_haw.ts \
                translations/deb-installer_he_IL.ts \
                translations/deb-installer_he.ts \
                translations/deb-installer_hi.ts \
                translations/deb-installer_hr.ts \
                translations/deb-installer_ht.ts \
                translations/deb-installer_hu.ts \
                translations/deb-installer_hy.ts \
                translations/deb-installer_id.ts \
                translations/deb-installer_is.ts \
                translations/deb-installer_it.ts \
                translations/deb-installer_ja.ts \
                translations/deb-installer_jv.ts \
                translations/deb-installer_ka.ts \
                translations/deb-installer_kk.ts \
                translations/deb-installer_km.ts \
                translations/deb-installer_kn.ts \
                translations/deb-installer_ko.ts \
                translations/deb-installer_ku.ts \
                translations/deb-installer_ky.ts \
                translations/deb-installer_lb.ts \
                translations/deb-installer_lo.ts \
                translations/deb-installer_lt.ts \
                translations/deb-installer_lv.ts \
                translations/deb-installer_mg.ts \
                translations/deb-installer_mi.ts \
                translations/deb-installer_mk.ts \
                translations/deb-installer_ml.ts \
                translations/deb-installer_mn.ts \
                translations/deb-installer_mr.ts \
                translations/deb-installer_ms.ts \
                translations/deb-installer_mt.ts \
                translations/deb-installer_my.ts \
                translations/deb-installer_nb_NO.ts \
                translations/deb-installer_nb.ts \
                translations/deb-installer_ne.ts \
                translations/deb-installer_nl_BE.ts \
                translations/deb-installer_nl.ts \
                translations/deb-installer_ny.ts \
                translations/deb-installer_or.ts \
                translations/deb-installer_pa.ts \
                translations/deb-installer_pl.ts \
                translations/deb-installer_ps.ts \
                translations/deb-installer_pt_BR.ts \
                translations/deb-installer_pt.ts \
                translations/deb-installer_ro.ts \
                translations/deb-installer_rue.ts \
                translations/deb-installer_ru_RU.ts \
                translations/deb-installer_ru.ts \
                translations/deb-installer_rw.ts \
                translations/deb-installer_sd.ts \
                translations/deb-installer_si.ts \
                translations/deb-installer_sk.ts \
                translations/deb-installer_sl.ts \
                translations/deb-installer_sm.ts \
                translations/deb-installer_sn.ts \
                translations/deb-installer_so.ts \
                translations/deb-installer_sq.ts \
                translations/deb-installer_sr.ts \
                translations/deb-installer_st.ts \
                translations/deb-installer_su.ts \
                translations/deb-installer_sv.ts \
                translations/deb-installer_sw.ts \
                translations/deb-installer_ta.ts \
                translations/deb-installer_te.ts \
                translations/deb-installer_tg.ts \
                translations/deb-installer_th.ts \
                translations/deb-installer_tk.ts \
                translations/deb-installer_tr.ts \
                translations/deb-installer_tt.ts \
                translations/deb-installer_ug.ts \
                translations/deb-installer_uk.ts \
                translations/deb-installer_ur.ts \
                translations/deb-installer_uz.ts \
                translations/deb-installer_vi.ts \
                translations/deb-installer_xh.ts \
                translations/deb-installer_yi.ts \
                translations/deb-installer_yo.ts \
                translations/deb-installer_yue_CN.ts \
                translations/deb-installer_zh_CN.ts \
                translations/deb-installer_zh_HK.ts \
                translations/deb-installer_zh_TW.ts
