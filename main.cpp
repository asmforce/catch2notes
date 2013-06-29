#include <QCoreApplication>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>

#include <QStringList>
#include <QString>
#include <QVariant>
#include <QMap>
#include <QList>
#include <QVector>

#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QTextCodec>

#include <iostream>



class Node
{
public:
  static const QString DateTimeFormat;
  enum  { Invalid = -1 };

public:
  Node() : mId(Invalid), mParentId(Invalid), mComments(0)
  {
  }

  ~Node()
  {
    if( mComments )  {
      delete mComments;
    }
  }

  void append( Node *comment )
  {
    if( ! mComments )  {
      mComments = new QList<Node*>();
    }
    // reverse an order: oldest first
    mComments->prepend( comment );
  }

  void out( QTextStream &stream )
  {
    stream << QDateTime::fromMSecsSinceEpoch(mCreationTime).toLocalTime().toString(DateTimeFormat) << endl;
    if( mModificationTime - mCreationTime >= 10*60*3600LL )  {
      stream << QDateTime::fromMSecsSinceEpoch(mModificationTime).toLocalTime().toString(DateTimeFormat) << endl;
    }

    stream << mText << endl;
    if( mComments )
    {
      foreach( Node *node, *mComments )
      {
        stream << "Upd (" << QDateTime::fromMSecsSinceEpoch(node->mCreationTime).toLocalTime().toString(DateTimeFormat) << ")" << endl;
        stream << node->mText << endl;
      }
    }
  }

public:
  qint32 mId, mParentId;
  QString mText;
  qint64 mCreationTime, mModificationTime;

protected:
  QList<Node*> *mComments;
};

const QString Node::DateTimeFormat = "dd.MM.yyyy hh:mm";


class Space
{
public:
  Space()
  {
  }

  ~Space()
  {
  }

  void append( Node *comment )
  {
    mNotes.append( comment );
  }

  void out( QTextStream &stream )
  {
    stream << "Space<" << mName << ">" << endl;
    stream << "// Notes: " << mNotes.size() << endl << endl;
    for( QList<Node*>::Iterator p = mNotes.begin(); p != mNotes.end(); ++p )  {
      (*p)->out( stream );
      stream << endl;
    }
    stream << endl;
  }

public:
  QString mName;
  QList<Node*> mNotes;
};



int main( int argc, char **argv )
{
  if( argc < 3 || argc > 3 )
  {
    if( argc == 2 && (strcmp(argv[1],"-h") == 0 || strcmp(argv[1],"--help") == 0) )
    {
      std::clog << "usage: notes <database-file> <output-file>" << std::endl;
    } else {
      std::clog << "error: use -h or --help for usage information" << std::endl;
    }
    return 0;
  }

  QSqlDatabase database = QSqlDatabase::addDatabase( "QSQLITE" );

  database.setDatabaseName( argv[1] );
  if( ! database.open() )  {
    std::cerr << "error: cannot open database file `" << argv[1] << "`" << std::endl;
    return 1;
  }

  QSqlQuery query;

  QList<Node> notes, comments;
  QMap< qint32, Node* > noteById;

  if( ! query.exec("SELECT _id, parent_id, created, timestamp, text FROM notes ORDER BY created DESC") )  {
    std::cerr << "query error: " << query.lastError().text().toLatin1().data() << std::endl;
    return 1;
  }

  while( query.next() )
  {
    Node node;

    node.mId = query.value(0).toInt();
    node.mParentId = query.value(1).toInt();

    node.mCreationTime = query.value(2).toLongLong();
    node.mModificationTime = query.value(3).toLongLong();
    node.mText = query.value(4).toString();

    if( node.mParentId == Node::Invalid )
    {
      notes.append( node );
      noteById.insert( node.mId, & notes.back() );
    } else {
      comments.append( node );
    }
  }

  // append comments to related notes
  for( QList<Node>::Iterator p = comments.begin(); p != comments.end(); ++p )
  {
    Node &commentNode = *p;
    noteById[commentNode.mParentId]->append( &commentNode );
  }

  if( ! query.exec("SELECT _id, stream_name FROM streams") )  {
    std::cerr << "query error: " << query.lastError().text().toLatin1().data() << std::endl;
    return 1;
  }

  QList<Space> spaces;
  QMap<qint32, Space*> spaceById;

  while( query.next() )
  {
    Space space;

    space.mName = query.value(1).toString();
    spaces.append( space );
    spaceById.insert( query.value(0).toInt(), & spaces.back() );
  }

  if( ! query.exec("SELECT note_id, stream_id FROM notes_streams") )  {
    std::cerr << "query error: " << query.lastError().text().toLatin1().data() << std::endl;
    return 1;
  }

  // fetch notes location (relating to an exact spaces) information
  QMultiMap<qint32, Space*> spaceByNoteId;
  while( query.next() )  {
    spaceByNoteId.insert( query.value(0).toInt(), spaceById[query.value(1).toInt()] );
  }

  // append notes to related spaces
  for( QList<Node>::Iterator np = notes.begin(); np != notes.end(); ++np )
  {
    QList<Space*> spaces = spaceByNoteId.values( np->mId );
    for( QList<Space*>::Iterator sp = spaces.begin(); sp != spaces.end(); ++sp )  {
      (*sp)->append( & *np );
    }
  }


  QFile outputFile( argv[2] );
  QTextStream stream( &outputFile );

  stream.setCodec( "UTF-8" );

  if( ! outputFile.open(QIODevice::WriteOnly) )  {
    std::cerr << "error: cannot write output file `" << argv[2] << "`" << std::endl;
    return 1;
  }

  for( QList<Space>::Iterator p = spaces.begin(); p != spaces.end(); ++p )  {
    p->out( stream );
    stream << endl;
  }

  stream.flush();
  outputFile.close();

  std::cout << "Spaces: " << spaces.size() << std::endl;
  std::cout << "Notes: " << notes.size() << std::endl;
  std::cout << "Comments: " << comments.size() << std::endl;

  return 0;
}
