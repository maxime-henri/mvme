/* mvme - Mesytec VME Data Acquisition
 *
 * Copyright (C) 2016-2018 mesytec GmbH & Co. KG <info@mesytec.com>
 *
 * Author: Florian Lüke <f.lueke@mesytec.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */
#ifndef __HISTO1D_WIDGET_H__
#define __HISTO1D_WIDGET_H__

#include "analysis/condition_editor_interface.h"
#include "histo1d.h"
#include "libmvme_export.h"

#include <QSpinBox>
#include <QWidget>

#include <qwt_plot_picker.h>

class QTextStream;
class QTimer;
class QwtPlotCurve;
class QwtPlotHistogram;
class QwtPlotPicker;
class QwtPlotTextLabel;
class QwtText;
class ScrollZoomer;
class MVMEContext;

namespace analysis
{
    class CalibrationMinMax;
    class Histo1DSink;
}

struct Histo1DWidgetPrivate;

class LIBMVME_EXPORT Histo1DWidget: public QWidget, public analysis::ConditionEditorInterface
{
    Q_OBJECT
    Q_INTERFACES(analysis::ConditionEditorInterface);

    public:
        using SinkPtr = std::shared_ptr<analysis::Histo1DSink>;
        using HistoSinkCallback = std::function<void (const SinkPtr &)>;

        // Convenience constructor that enables the widget to keep the histo alive.
        Histo1DWidget(const Histo1DPtr &histo, QWidget *parent = 0);
        Histo1DWidget(Histo1D *histo, QWidget *parent = 0);
        virtual ~Histo1DWidget();

        void setHistogram(const Histo1DPtr &histo);
        void setHistogram(Histo1D *histo);

        virtual bool eventFilter(QObject *watched, QEvent *e) override;
        virtual bool event(QEvent *event) override;

        friend class Histo1DListWidget;

        void setContext(MVMEContext *context);
        void setCalibrationInfo(const std::shared_ptr<analysis::CalibrationMinMax> &calib,
                                s32 histoAddress);
        void setSink(const SinkPtr &sink, HistoSinkCallback sinkModifiedCallback);

        void setResolutionReductionFactor(u32 rrf);
        void setResolutionReductionSliderEnabled(bool b);

        //QwtPlotCurve *getPlotCurve() { return m_plotCurve; }

        virtual bool editCondition(const analysis::ConditionLink &cl) override;
        virtual analysis::ConditionLink getCondition() const override;

    public slots:
        void replot();

    private slots:
        /* IMPORTANT: leave slots invoked by qwt here for now. do not use lambdas!
         * Reason: there is/was a bug where qwt signals could only be succesfully
         * connected using the old SIGNAL/SLOT macros. Newer function pointer based
         * connections did not work. */
        void zoomerZoomed(const QRectF &);
        void mouseCursorMovedToPlotCoord(QPointF);
        void mouseCursorLeftPlot();

        void on_tb_subRange_clicked();
        void on_tb_rate_toggled(bool checked);
        void on_tb_gauss_toggled(bool checked);
        void on_tb_test_clicked();
        void on_ratePointerPicker_selected(const QPointF &);

    protected:
        virtual void paintEvent(QPaintEvent *event) override;

    private:
        std::unique_ptr<Histo1DWidgetPrivate> m_d;
        friend struct Histo1DWidgetPrivate;
};

class Histo1DListWidget: public QWidget, public analysis::ConditionEditorInterface
{
    Q_OBJECT
    Q_INTERFACES(analysis::ConditionEditorInterface);

    public:
        using HistoList = QVector<std::shared_ptr<Histo1D>>;
        using SinkPtr = Histo1DWidget::SinkPtr;
        using HistoSinkCallback = Histo1DWidget::HistoSinkCallback;

        Histo1DListWidget(const HistoList &histos, QWidget *parent = 0);
        virtual ~Histo1DListWidget();

        HistoList getHistograms() const;

        void setContext(MVMEContext *context);
        void setCalibration(const std::shared_ptr<analysis::CalibrationMinMax> &calib);
        void setSink(const SinkPtr &sink, HistoSinkCallback sinkModifiedCallback);
        void selectHistogram(int histoIndex);

        // ConditionEditorInterface
        virtual bool editCondition(const analysis::ConditionLink &cl) override;
        virtual analysis::ConditionLink getCondition() const override;

    private:
        struct Private;
        std::unique_ptr<Private> m_d;
        void onHistoSpinBoxValueChanged(int index);

};

class PickerOverlayTest: public QwtPlotPicker
{
    Q_OBJECT
    public:
        PickerOverlayTest(QWidget *canvas);

        PickerOverlayTest(int xAxis, int yAxis,
                          RubberBand rubberBand,
                          DisplayMode trackerMode,
                          QWidget *canvas);

        virtual ~PickerOverlayTest() override;

        virtual void drawRubberBand(QPainter *painter) const override;
        virtual void drawTracker(QPainter *painter) const override;

        void editCondition(const analysis::ConditionLink &cl);

    protected:
        virtual void move(const QPoint &p) override;
        virtual void append(const QPoint &p) override;
        virtual bool end(bool ok = true) override;
        virtual void widgetMousePressEvent(QMouseEvent *) override;
        virtual void widgetMouseReleaseEvent(QMouseEvent *) override;
        virtual void widgetMouseMoveEvent(QMouseEvent *) override;

    private:
        analysis::ConditionLink m_cl;
        bool m_isDragging = false;
        enum IntervalBorderType { None, Min, Max };
        IntervalBorderType m_borderEditType;
};

#endif /* __HISTO1D_WIDGET_H__ */
