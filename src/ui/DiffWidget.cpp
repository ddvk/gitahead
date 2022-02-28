//
//          Copyright (c) 2016, Scientific Toolworks, Inc.
//
// This software is licensed under the MIT License. The LICENSE.md file
// describes the conditions under which this software may be distributed.
//
// Author: Jason Haslam
//

#include "DiffWidget.h"
#include "DiffView.h"
#include "FileList.h"
#include "FindWidget.h"
#include "RepoView.h"
#include "conf/Settings.h"
#include "git/Commit.h"
#include "git/Diff.h"
#include "git/Index.h"
#include <QScrollBar>
#include <QShortcut>
#include <QSplitter>
#include <QVBoxLayout>

namespace {

const QItemSelectionModel::SelectionFlags kSelectionFlags =
  QItemSelectionModel::Current;

} // anon. namespace

void DiffWidget::focus(){
    mFiles->setFocus();
}

DiffWidget::DiffWidget(const git::Repository &repo, QWidget *parent)
  : ContentWidget(parent)
{

  QWidget *view2 = new QWidget();
  mFiles = new FileList(repo, view2);
  mFiles->hide(); // Start hidden.

  mDiffView = new DiffView(repo, this);

  mFind = new FindWidget(mDiffView, this);
  mFind->hide(); // Start hidden.

  QWidget *view = new QWidget(this);
  QVBoxLayout *viewLayout = new QVBoxLayout(view);
  viewLayout->setContentsMargins(0,0,0,0);
  viewLayout->setSpacing(0);
  viewLayout->addWidget(mFind);
  viewLayout->addWidget(mDiffView);

  auto *viewLayout2 = new QVBoxLayout(view2);
  viewLayout2->setContentsMargins(0,0,0,0);
  viewLayout2->setSpacing(0);
  viewLayout2->addWidget(mFiles);
  viewLayout2->addStretch(0);

  mSplitter = new QSplitter(Qt::Horizontal);
  mSplitter->setChildrenCollapsible(false);
  mSplitter->setHandleWidth(0);
  mSplitter->addWidget(view2);
  mSplitter->addWidget(view);
  mSplitter->setSizes(QList<int>({100, 300}));
//  mSplitter->setStretchFactor(0, 1);
//  mSplitter->setStretchFactor(1, 3);

  auto shortcut = new QShortcut(QKeySequence(tr("Alt+2", "DiffView")), parent);
  connect(shortcut, &QShortcut::activated, [this]{
      mFiles->setFocus();
  });
  connect(mFiles, &FileList::gotoCommits, [this]{
      QWidget *w = qobject_cast<QWidget*>(this->parent()->parent());
      w->setFocus();
  });

//  setFocusProxy(mFiles);

//  connect(mSplitter, &QSplitter::splitterMoved, [this] {
//    FileList::setFileRows(mSplitter->height()*2 / mFiles->sizeHintForRow(0));
//  });

  QVBoxLayout *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0,0,0,0);
  layout->setSpacing(0);
  layout->setMargin(0);
  layout->addWidget(mSplitter);

//  this->layout()->setSpacing(0);
//  this->layout()->setContentsMargins(0,0,0, 0);
  // Filter on selection change.
  QItemSelectionModel *selection = mFiles->selectionModel();
  connect(selection, &QItemSelectionModel::selectionChanged, [this] {
    QStringList paths;
    QModelIndexList indexes = mFiles->selectionModel()->selectedIndexes();
    foreach (const QModelIndex &index, indexes)
      paths.append(index.data().toString());
    mDiffView->setFilter(paths);
  });

  connect(mDiffView->verticalScrollBar(), &QScrollBar::valueChanged,
          this, &DiffWidget::setCurrentFile);

  connect(mFiles, &FileList::sortRequested, [this] {
    setDiff(mDiff);
  });
}

QString DiffWidget::selectedFile() const
{
  QModelIndexList indexes = mFiles->selectionModel()->selectedIndexes();
  return indexes.isEmpty() ? QString() :
         indexes.first().data(Qt::DisplayRole).toString();
}

void DiffWidget::setDiff(
  const git::Diff &diff,
  const QString &file,
  const QString &pathspec)
{
  mDiff = diff;

  // Cancel find.
  mFind->hide();

  // Populate views.
  if (mDiff.isValid()) {
    Qt::SortOrder order = Qt::DescendingOrder;
    git::Diff::SortRole role = git::Diff::StatusRole;
    if (!mDiff.isConflicted()) {
      Settings *settings = Settings::instance();
      role = static_cast<git::Diff::SortRole>(settings->value("sort/role").toInt());
      order = static_cast<Qt::SortOrder>(settings->value("sort/order").toInt());
    }

    mDiff.sort(role, order);
  }

  mDiffView->setDiff(diff);
  mFiles->setDiff(diff, pathspec);

  // Reset find.
  mFind->reset();

  // Adjust splitter.
//  mSplitter->setSizes({mFiles->sizeHint().height(), -1});

  // Scroll to file.
  selectFile(file);
}

void DiffWidget::find()
{
  mFind->showAndSetFocus();
}

void DiffWidget::findNext()
{
  mFind->find();
}

void DiffWidget::findPrevious()
{
  mFind->find(FindWidget::Backward);
}

void DiffWidget::selectFile(const QString &file)
{
  if (file.isEmpty())
    return;

  QAbstractItemModel *model = mFiles->model();
  QModelIndex start = model->index(0, 0);
  QModelIndexList indexes = model->match(start, Qt::DisplayRole, file);

  // The file might not be found if it was renamed.
  // FIXME: Look up the old name from the blame?
  if (!indexes.isEmpty()) {
    QModelIndex index = indexes.first();
    if (mDiffView->scrollToFile(index.row()))
      mFiles->selectionModel()->select(index, kSelectionFlags);
  }
}

void DiffWidget::setCurrentFile(int value)
{
  // FIXME: This is bogus.
  if (!mDiffView->widget())
    return;

  QAbstractItemModel *model = mFiles->model();
  int rows = model->rowCount();
  for (int i = 0; i < rows; ++i) {
    if (!mDiffView->file(i)->isVisible())
      continue;

    // Get the position of the next widget.
    int pos = mDiffView->widget()->height();
    if (i < rows - 1)
      pos = mDiffView->file(i + 1)->y();

    // Stop at the first index where the scroll
    // value is less than the next widget.
    if (value < pos) {
      QModelIndex index = model->index(i, 0);
      mFiles->selectionModel()->select(index, kSelectionFlags);
      mFiles->scrollTo(index);
      break;
    }
  }
}
