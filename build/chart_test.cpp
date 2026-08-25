#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChartView>
#include <QtCharts/QValueAxis>
using namespace QtCharts;
int main() {
    QChart *c = new QChart();
    QLineSeries *s = new QLineSeries(c);
    QChartView *v = new QChartView(c, nullptr);
    QValueAxis *a = new QValueAxis();
    (void)s; (void)v; (void)a;
    return 0;
}
