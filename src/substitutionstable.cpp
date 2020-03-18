#include "substitutionstable.h"
#include "debug.h"
#include "languagecodes.h"
#include "tesseract.h"

#include <QComboBox>
#include <QPainter>
#include <QStringListModel>
#include <QStyledItemDelegate>

namespace
{
class SubstitutionDelegate : public QStyledItemDelegate
{
public:
  explicit SubstitutionDelegate(QObject *parent = nullptr)
    : QStyledItemDelegate(parent)
  {
  }

  void paint(QPainter *painter, const QStyleOptionViewItem &option,
             const QModelIndex &index) const override
  {
    painter->drawText(option.rect, Qt::AlignCenter, index.data().toString());
  }
};

}  // namespace

SubstitutionsTable::SubstitutionsTable(QWidget *parent)
  : QTableWidget(parent)
  , languagesModel_(new QStringListModel(this))
{
  setItemDelegate(new SubstitutionDelegate(this));
  setColumnCount(int(Column::Count));
  setHorizontalHeaderLabels({tr("Language"), tr("Source"), tr("Changed")});
  connect(this, &SubstitutionsTable::itemChanged,  //
          this, &SubstitutionsTable::handleItemChange);
}

void SubstitutionsTable::setSubstitutions(const Substitutions &substitutions)
{
  setRowCount(0);

  updateModel(substitutions);

  for (const auto &i : substitutions) addRow(i.first, i.second);

  addRow();  // for editing
  resizeColumnsToContents();
}

void SubstitutionsTable::setTessdataPath(const QString &tessdataPath)
{
  tessdataPath_ = tessdataPath;
  if (rowCount() == 0)  // must be at least 1 if inited
    return;
  setSubstitutions(substitutions());
}

void SubstitutionsTable::updateModel(const Substitutions &substitutions)
{
  auto strings = Tesseract::availableLanguageNames(tessdataPath_);

  if (!substitutions.empty()) {
    LanguageCodes languages;
    for (const auto &i : substitutions) {
      auto name = i.first;
      if (const auto bundle = languages.findByTesseract(name))
        name = QObject::tr(bundle->name);

      if (!strings.contains(name))
        strings.append(name);
    }
  }

  std::sort(strings.begin(), strings.end());
  languagesModel_->setStringList(strings);
}

Substitutions SubstitutionsTable::substitutions() const
{
  Substitutions result;
  for (auto row = 0, end = rowCount(); row < end; ++row) {
    const auto pair = at(row);
    SOFT_ASSERT(!pair.first.isEmpty(), continue);
    if (pair.second.source.isEmpty())
      continue;
    result.emplace(pair.first, pair.second);
  }
  return result;
}

void SubstitutionsTable::addRow(const LanguageId &language,
                                const Substitution &substutution)
{
  QSignalBlocker blocker(this);
  auto row = rowCount();
  insertRow(row);

  auto combo = new QComboBox(this);
  combo->setModel(languagesModel_);

  using E = Column;
  if (!language.isEmpty()) {
    LanguageCodes langs;
    if (auto lang = langs.findById(language))
      combo->setCurrentText(QObject::tr(lang->name));
    else
      combo->setCurrentText(language);
  } else if (rowCount() > 1) {
    const auto previousRow = rowCount() - 2;
    auto previousCombo =
        static_cast<QComboBox *>(cellWidget(previousRow, int(E::Language)));
    SOFT_ASSERT(previousCombo, return );
    combo->setCurrentText(previousCombo->currentText());
  }

  setCellWidget(row, int(E::Language), combo);
  setItem(row, int(E::Source), new QTableWidgetItem(substutution.source));
  setItem(row, int(E::Target), new QTableWidgetItem(substutution.target));
}

std::pair<LanguageId, Substitution> SubstitutionsTable::at(int row) const
{
  SOFT_ASSERT(row >= 0 && row < rowCount(), return {});

  using E = Column;
  auto combo = static_cast<QComboBox *>(cellWidget(row, int(E::Language)));
  SOFT_ASSERT(combo, return {});

  LanguageCodes langs;
  auto lang = langs.findByName(combo->currentText());
  SOFT_ASSERT(lang, return {});

  Substitution sub;
  auto sourceItem = item(row, int(E::Source));
  SOFT_ASSERT(sourceItem, return {});
  sub.source = sourceItem->text();

  auto targetItem = item(row, int(E::Target));
  SOFT_ASSERT(targetItem, return {});
  sub.target = targetItem->text();

  return std::make_pair(lang->id, sub);
}

void SubstitutionsTable::handleItemChange(QTableWidgetItem *item)
{
  if (item->column() != int(Column::Source))
    return;

  const auto lastRow = rowCount() - 1;
  if (!item->text().isEmpty() && item->row() == lastRow) {
    addRow();
    return;
  }

  if (item->text().isEmpty() && item->row() != lastRow) {
    removeRow(item->row());
    return;
  }
}
