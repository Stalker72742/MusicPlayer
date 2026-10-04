#include "SuggestionListModel.h"

int SuggestionListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : GetCount();
}

QVariant SuggestionListModel::data(const QModelIndex& index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid))
        return {};

    const Suggestion& suggestion = suggestions.at(index.row());
    switch (role)
    {
        case KindRole:       return suggestion.kind;
        case TitleRole:      return suggestion.title;
        case SubtitleRole:   return suggestion.subtitle;
        case IconRole:       return suggestion.icon;
        case TagRole:        return suggestion.tag;
        case CompletionRole: return suggestion.completion;
        default:             return {};
    }
}

QHash<int, QByteArray> SuggestionListModel::roleNames() const
{
    return {
        {KindRole, "kind"},
        {TitleRole, "title"},
        {SubtitleRole, "subtitle"},
        {IconRole, "icon"},
        {TagRole, "tag"},
        {CompletionRole, "completion"},
    };
}

const Suggestion* SuggestionListModel::At(int row) const
{
    return row >= 0 && row < suggestions.size() ? &suggestions[row] : nullptr;
}

void SuggestionListModel::SetSuggestions(const QList<Suggestion>& newSuggestions)
{
    const bool bCountChanged = newSuggestions.size() != suggestions.size();

    beginResetModel();
    suggestions = newSuggestions;
    endResetModel();

    if (bCountChanged)
        emit countChanged();
}
